/*
 * This file is part of gray, a free, GPU accelerated, realtime pathtracing engine,
 * Copyright (C) 2016  Aksel Alpay
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FRAME_RENDERER_HPP
#define FRAME_RENDERER_HPP

#include "scene.hpp"
#include "timer.hpp"
#include "qcl.hpp"
#include "random.hpp"
#include "reduction.hpp"
#include "common.cl_hpp"

#include <cstdint>
#include <array>

namespace gray {

class frame_renderer
{
public:
  frame_renderer(const qcl::device_context_ptr& ctx,
                const std::string& kernel_name,
                const std::string& post_processor_name,
                std::size_t render_width,
                std::size_t render_height,
                std::size_t random_seed = device_object::random_engine::generate_seed())
  : _target_fps{24.0}, _current_fps{0.0}, _num_rays_ppx{10}, _ctx{ctx},
    _width(render_width), _height(render_height),
    _kernel{ctx->get_kernel(kernel_name)},
    _post_processing_kernel{ctx->get_kernel(post_processor_name)},
    _total_num_rays{0},
    _kernel_run_event{1},
    _frame_number{0},
    _image_max_reduction{ctx}
  {
    set_resolution(render_width, render_height, random_seed);

    std::vector<cl_float> max_running_average_init{_max_value_running_average_size, 1.0f};

    //_ctx->require_several_command_queues(2);

    _ctx->create_buffer<cl_float>(_max_value_running_average, 
                                  CL_MEM_READ_WRITE,
                                  _max_value_running_average_size, 
                                  max_running_average_init.data());
  }

  void set_target_fps(double fps)
  {
    _target_fps = fps;
  }

  void set_target_rendering_time(double time)
  {
    _target_fps = 1.0 / time;
  }

  scalar get_target_fps() const
  {
    return _target_fps;
  }

  scalar get_current_fps() const
  {
    return _current_fps;
  }

  void set_resolution(std::size_t width, 
                      std::size_t height, 
                      std::size_t seed = device_object::random_engine::generate_seed())
  {
    _ctx->get_command_queue().finish();

    auto work_items = get_required_num_work_items(width, height);
    this->_random = device_object::random_engine{
        _ctx, work_items[0], work_items[1], seed
    };

    _buffer_a = create_image_buffer(width, height);
    _buffer_b = create_image_buffer(width, height);
    _image_max_reduction.set_resolution(width, height);

    _total_num_rays = 0;

    _width = width;
    _height = height;
  }

  std::size_t get_resolution_width() const
  {
    return _width;
  }

  std::size_t get_resolution_height() const
  {
    return _height;
  }

  void discard_render_results()
  {
    this->_total_num_rays = 0;
  }

  std::uint_fast64_t get_frame_number() const
  {
    return _frame_number;
  }

  qcl::device_context_ptr get_cl_context() const
  {
    return _ctx;
  }

  void skip_frame()
  {
    ++_frame_number;
  }

  template<class Image_type>
  void render(const Image_type& pixels,
              const device_object::scene& s, 
              const device_object::camera& cam)
  {
    ++_frame_number;

    if(!_timer.is_running())
      _timer.start();

    if(_total_num_rays > 100000)
      return;

    // Size of work group must divide number of work items
    auto work_items = get_required_num_work_items(_width, _height);
    cl_int err;

    //Call kernel
    _kernel->setArg(0, *_buffer_a);
    _kernel->setArg(1, *_buffer_b);
    _kernel->setArg(2, static_cast<cl_int>(_total_num_rays));
    _kernel->setArg(3, _random.get());
    _kernel->setArg(4, sizeof(cl_float) * _work_group_size * _work_group_size, nullptr);
    _kernel->setArg(5, sizeof(device_object::camera), &cam);
    _kernel->setArg(6, _num_rays_ppx);

    _kernel->setArg(7, s.get_objects());
    _kernel->setArg(8, s.get_spheres());
    _kernel->setArg(9, s.get_planes());
    _kernel->setArg(10, s.get_disks());
    _kernel->setArg(11, static_cast<cl_int>(s.get_num_spheres()));
    _kernel->setArg(12, static_cast<cl_int>(s.get_num_planes()));
    _kernel->setArg(13, static_cast<cl_int>(s.get_num_disks()));
    _kernel->setArg(14, s.get_far_clipping_distance());

    _kernel->setArg(15, s.get_materials().get_scattered_fraction());
    _kernel->setArg(16, s.get_materials().get_emitted_light());
    _kernel->setArg(17, s.get_materials().get_transmittance_refraction_specular());
    _kernel->setArg(18, s.get_materials().get_widths());
    _kernel->setArg(19, s.get_materials().get_heights());
    _kernel->setArg(20, s.get_materials().get_offsets());
    _kernel->setArg(21, static_cast<cl_int>(s.get_materials().get_num_material_maps()));

    assert(_kernel_run_event.size() == 1);

    err = _ctx->get_command_queue().enqueueNDRangeKernel(*_kernel,
                                                         cl::NullRange,
                                                         cl::NDRange(work_items[0], work_items[1]),
                                                         cl::NDRange(_work_group_size, _work_group_size),
                                                         nullptr,
                                                         &(_kernel_run_event[0]));

    qcl::check_cl_error(err, "Could not enqueue kernel call!");

    // Obtain maximum pixel value. This is required for the
    // color range compression during post processing.
    _image_max_reduction.run_reduction(*_buffer_a);

    _post_processing_kernel->setArg(0, pixels);
    _post_processing_kernel->setArg(1, *_buffer_a);
    _post_processing_kernel->setArg(2, _image_max_reduction.get_reduction_result());
    _post_processing_kernel->setArg(3, _max_value_running_average);
    _post_processing_kernel->setArg(4, static_cast<cl_ulong>(_frame_number));
    _post_processing_kernel->setArg(5, static_cast<cl_int>(get_smoothing_size()));

    cl::Event post_processor_run;
    err = _ctx->get_command_queue(0).enqueueNDRangeKernel(*_post_processing_kernel,
                                                          cl::NullRange,
                                                          cl::NDRange(work_items[0], work_items[1]),
                                                          cl::NDRange(_work_group_size, _work_group_size),
                                                          nullptr,
                                                          &post_processor_run);
    qcl::check_cl_error(err, "Could not enqueue postprocessing kernel call!");

    double time = _timer.stop();
    _timer.start();

    _total_num_rays += _num_rays_ppx;

    _current_fps = 1.0 / time;
    _num_rays_ppx = static_cast<portable_int>(std::round(_num_rays_ppx * _current_fps /
                                                         _target_fps));
    if (_num_rays_ppx < 1)
      _num_rays_ppx = 1;

    std::swap(_buffer_a, _buffer_b);

    std::cout << "Performance @ " << static_cast<double>(_width * _height * _num_rays_ppx) / (1.e6 * time)
              << " Mrays/s, num_rays_ppx=" << _num_rays_ppx << " fps=" << _current_fps << std::endl;
  }

  const qcl::device_context_ptr& get_current_context() const
  {
    return _ctx;
  }

  portable_int get_current_rays_per_pixel() const
  {
    return _num_rays_ppx;
  }

private:
  inline int get_smoothing_size() const
  {
    const double max_smoothing = 10.0;

    return static_cast<int>(max_smoothing / (0.1 * static_cast<double>(_total_num_rays) + 1.0));
  }

  inline
  std::array<std::size_t,2> get_required_num_work_items(std::size_t width, std::size_t height)
  {
    std::size_t effective_width = width;
    std::size_t effective_height = height;
    if(effective_width % _work_group_size != 0)
      effective_width = (_width / _work_group_size + 1) * _work_group_size;
    if(effective_height % _work_group_size != 0)
      effective_height = (_height / _work_group_size + 1) * _work_group_size;
    return {effective_width, effective_height};
  }

  std::shared_ptr<cl::Image2D> create_image_buffer(std::size_t width, 
                                                  std::size_t height) const
  {
    cl_int err;
    auto ptr = std::make_shared<cl::Image2D>(
        _ctx->get_context(),
        CL_MEM_READ_WRITE,
        cl::ImageFormat{CL_RGBA, CL_FLOAT},
        width, height, 0, nullptr, &err);

    qcl::check_cl_error(err, "Could not create CL image object!");

    cl::Event buffer_fill;

    cl::size_t<3> range;
    range[0] = width;
    range[1] = height;
    range[2] = 1;

    cl_float4 fill_value = {{0.0f, 0.0f, 0.0f, 0.0f}};
    _ctx->get_command_queue().enqueueFillImage(*ptr,
                                               fill_value,
                                               cl::size_t<3>{},
                                               range,
                                               nullptr, &buffer_fill);
    buffer_fill.wait();

    return ptr;
  }

  double _target_fps;
  double _current_fps;
  portable_int _num_rays_ppx;
  qcl::device_context_ptr _ctx;

  std::size_t _width;
  std::size_t _height;

  device_object::random_engine _random;

  qcl::kernel_ptr _kernel;
  qcl::kernel_ptr _post_processing_kernel;

  static constexpr std::size_t _work_group_size = 8;

  std::shared_ptr<cl::Image2D> _buffer_a;
  std::shared_ptr<cl::Image2D> _buffer_b;

  std::size_t _total_num_rays;

  std::vector<cl::Event> _kernel_run_event;

  timer _timer;

  std::uint_fast64_t _frame_number;

  image_maximum_value _image_max_reduction;

  static constexpr std::size_t _max_value_running_average_size = 
                                    MAX_VALUE_RUNNING_AVERAGE_SIZE;
  cl::Buffer _max_value_running_average;
};
}

#endif
