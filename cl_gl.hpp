/*
 * This file is part of mandelgpu, a free GPU accelerated fractal viewer,
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


#ifndef CL_GL_HPP
#define CL_GL_HPP


#include <CL/cl.hpp>
#include <functional>
#include <GL/gl.h>

class gl_renderer;

class cl_gl
{
public:
  static void init_environment();
  
  cl_gl(const gl_renderer* r, const cl::Context& context);
  ~cl_gl();
  
  typedef std::function<void (const cl::ImageGL&, std::size_t, std::size_t)> 
        kernel_executor_type;
  
  void display(kernel_executor_type kernel_call, cl::CommandQueue& queue);
  
  void rebuild_buffers();
private:
  void init();
  void release();
 
  const gl_renderer* _renderer;
  GLuint _texture;
  cl::Context _context;
  
  cl::ImageGL _cl_buffer;
  std::vector<cl::Memory> _gl_objects;
};

#endif
