#ifndef FRAME_RENDERER_HPP
#define FRAME_RENDERER_HPP

#include "scene.hpp"
#include "timer.hpp"
#include "qcl.hpp"
#include "random.hpp"

namespace gray {

class frame_renderer
{
public:
  frame_renderer(const qcl::device_context_ptr& ctx,
                const std::string& kernel_name,
                std::size_t render_width,
                std::size_t render_height,
                std::size_t random_seed = device_object::random_engine::generate_seed())
  : _target_fps{24.0}, _current_fps{0.0}, _num_rays_ppx{10}, _ctx{ctx},
    _width(render_width), _height(render_height),
    _random{ctx, render_width, render_height, random_seed},
    _kernel{ctx->get_kernel(kernel_name)}
  {}

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

  void set_resolution(std::size_t width, std::size_t height)
  {
    std::size_t seed = device_object::random_engine::generate_seed();
    this->_random = device_object::random_engine{
        _ctx, width, height, seed};

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

  template<class Image_type>
  void render(const Image_type& pixels,
              const device_object::scene& s, 
              const device_object::camera& cam)
  {
    timer t;

    cl::Event event;
    t.start();

    //Call kernel
    _kernel->setArg(0, pixels);
    _kernel->setArg(1, _random.get());
    _kernel->setArg(2, sizeof(device_object::camera), &cam);
    _kernel->setArg(3, _num_rays_ppx);

    _kernel->setArg(4, s.get_objects());
    _kernel->setArg(5, s.get_spheres());
    _kernel->setArg(6, s.get_planes());
    _kernel->setArg(7, s.get_disks());
    _kernel->setArg(8, s.get_num_spheres());
    _kernel->setArg(9, s.get_num_planes());
    _kernel->setArg(10, s.get_num_disks());
    _kernel->setArg(11, s.get_far_clipping_distance());

    _kernel->setArg(12, s.get_materials().get_scattered_fraction());
    _kernel->setArg(13, s.get_materials().get_emitted_light());
    _kernel->setArg(14, s.get_materials().get_transmittance_refraction_specular());
    _kernel->setArg(15, s.get_materials().get_widths());
    _kernel->setArg(16, s.get_materials().get_heights());
    _kernel->setArg(17, s.get_materials().get_offsets());
    _kernel->setArg(18, s.get_materials().get_num_material_maps());

    std::cout << "Starting kernel, nrays ppx = " << _num_rays_ppx << std::endl;

    // Size of work group must divide number of work items
    std::size_t effective_width = _width;
    std::size_t effective_height = _height;
    if(effective_width % _work_group_size != 0)
      effective_width = (_width / _work_group_size + 1) * _work_group_size;
    if(effective_height % _work_group_size != 0)
      effective_height = (_height / _work_group_size + 1) * _work_group_size;

    cl_int err = _ctx->get_command_queue().enqueueNDRangeKernel(*_kernel,
                                                                cl::NullRange,
                                                                cl::NDRange(effective_width, effective_height),
                                                                cl::NDRange(_work_group_size, _work_group_size),
                                                                NULL,
                                                                &event);
    qcl::check_cl_error(err, "Could not enqueue kernel call!");

    event.wait();

    double time = t.stop();

    _current_fps = 1.0 / time;
    _num_rays_ppx = static_cast<portable_int>(_num_rays_ppx /
                                              (time * _target_fps));
    if(_num_rays_ppx < 200)
      _num_rays_ppx = 200;
    std::cout << time << " " << _current_fps << " " << _num_rays_ppx << std::endl;
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
  double _target_fps;
  double _current_fps;
  portable_int _num_rays_ppx;
  qcl::device_context_ptr _ctx;

  std::size_t _width;
  std::size_t _height;

  device_object::random_engine _random;

  qcl::kernel_ptr _kernel;

  static constexpr std::size_t _work_group_size = 8;
};
}

#endif
