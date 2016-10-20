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

#ifndef REDUCTION_HPP
#define REDUCTION_HPP

#include "qcl.hpp"

namespace gray {

class image_maximum_value
{
public:
  image_maximum_value(const qcl::device_context_ptr& ctx)
  : _ctx{ctx},
    _init_kernel{ctx->get_kernel("max_value_reduction_init")},
    _reduction_kernel{ctx->get_kernel("max_value_reduction")},
    _buffer_size{0},
    _wait_events{1}
  {
  }

  void set_resolution(std::size_t width, std::size_t height)
  {
    std::size_t required_buffer_size =
        get_required_num_work_items(width * height);

    _buffer = _ctx->create_buffer<cl_float>(CL_MEM_READ_WRITE, required_buffer_size);

    _buffer_size = required_buffer_size;
  }

  void run_reduction(const cl::Image2D& input)
  {
    std::size_t image_width, image_height;
    input.getImageInfo(CL_IMAGE_WIDTH, &image_width);
    input.getImageInfo(CL_IMAGE_HEIGHT, &image_height);

    assert(_buffer_size != 0);
    assert(_buffer_size >= image_width * image_height);

    std::size_t work_items_x =
        get_required_num_work_items(image_width, _img_group_size2d);
    std::size_t work_items_y =
        get_required_num_work_items(image_height, _img_group_size2d);

   

    _init_kernel->setArg(0, input);
    _init_kernel->setArg(1, *_buffer);

    cl_int err;

    err = _ctx->get_command_queue().enqueueNDRangeKernel(*_init_kernel,
                                                   cl::NullRange,
                                                   cl::NDRange(work_items_x, work_items_y),
                                                   cl::NDRange(_img_group_size2d,_img_group_size2d),
                                                   nullptr,
                                                   &(_wait_events[0]));
    qcl::check_cl_error(err, "Could not enqueue init kernel for reduction!");

    _reduction_kernel->setArg(0, *_buffer);
    _reduction_kernel->setArg(1, _group_size * sizeof(cl_float), nullptr);

    cl::Event event;
    err = _ctx->get_command_queue().enqueueNDRangeKernel(*_reduction_kernel,
                                                         cl::NullRange,
                                                         cl::NDRange(_buffer_size),
                                                         cl::NDRange(_group_size),
                                                         &_wait_events,
                                                         &event);

    qcl::check_cl_error(err, "Could not enqueue kernel for reduction!");

    event.wait();

    if(_buffer_size > _group_size)
    {
      // We need to redcue the result of each group to find the global
      // result

      std::size_t num_groups = _buffer_size;

      do
      {
        num_groups = get_required_num_work_items(num_groups / _group_size);

        err = _ctx->get_command_queue().enqueueNDRangeKernel(*_reduction_kernel,
                                                             cl::NullRange,
                                                             cl::NDRange(num_groups),
                                                             cl::NDRange(_group_size),
                                                             nullptr,
                                                             &event);

        qcl::check_cl_error(err, "Could not enqueue kernel for final reduction step!");

        event.wait();

      } while (num_groups > _group_size);
    }
  }

  const cl::Buffer& get_reduction_result() const
  {
    return *_buffer;
  }

private:


  inline
  std::size_t get_required_num_work_items(std::size_t num_items, 
                                          std::size_t group_size = _group_size) const
  {
    if(num_items % group_size != 0)
      return (num_items / group_size + 1) * group_size;
    return num_items;
  }

  qcl::device_context_ptr _ctx;
  qcl::kernel_ptr _init_kernel;
  qcl::kernel_ptr _reduction_kernel;

  std::size_t _buffer_size;
  qcl::buffer_ptr _buffer;

  static constexpr std::size_t _group_size = 512;

  static constexpr std::size_t _img_group_size2d = 8;

  std::vector<cl::Event> _wait_events;
};
}

#endif
