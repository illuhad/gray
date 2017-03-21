/*
 * This file is part of gray, a free, GPU accelerated, realtime pathtracing
 * engine,
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

#include "cl_gl.hpp"
#include "gl_renderer.hpp"

// Must come before gl.h
#include <GL/glew.h>

#include <GL/gl.h>

#include <cassert>
#include <iostream>

void cl_gl::init_environment() { glewInit(); }

cl_gl::cl_gl(const gl_renderer* r, const cl::Context& context, bool gl_sharing)
    : _renderer(r), _gl_sharing(gl_sharing), _context(context)
{
  init();
}

cl_gl::~cl_gl() { release(); }

void cl_gl::rebuild_buffers()
{
  release();
  init();
}

void cl_gl::release() { glDeleteTextures(1, &_texture); }

void cl_gl::init()
{
  glGenTextures(1, &_texture);

  glBindTexture(GL_TEXTURE_2D, _texture);

  // set basic parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               static_cast<GLsizei>(_renderer->get_width()),
               static_cast<GLsizei>(_renderer->get_height()), 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);

  cl_int err;
  if (_gl_sharing)
  {
    this->_cl_buffer = std::make_shared<cl::ImageGL>(
        _context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, _texture, &err);
  }
  else
  {
    this->_cl_buffer = std::make_shared<cl::Image2D>(
        _context, CL_MEM_READ_WRITE, cl::ImageFormat{CL_RGBA, CL_UNORM_INT8},
        _renderer->get_width(), _renderer->get_height(), 0, nullptr, &err);
    this->_host_cl_buffer.resize(4 * _renderer->get_width() *
                                 _renderer->get_height());
  }

  if (err != CL_SUCCESS)
    throw std::runtime_error("Could not create Image!");

  this->_gl_objects.clear();
  this->_gl_objects.push_back(*_cl_buffer);

  glBindTexture(GL_TEXTURE_2D, 0);
  glFinish();
  assert(glGetError() == GL_NO_ERROR);
}

void cl_gl::display(kernel_executor_type kernel_call, cl::CommandQueue& queue)
{

  // Call Kernel
  glFinish();

  if (this->_gl_sharing)
  {
    cl_int err = queue.enqueueAcquireGLObjects(&_gl_objects, NULL, NULL);
    assert(err == CL_SUCCESS);

    kernel_call(*(this->_cl_buffer), _renderer->get_width(),
                _renderer->get_height());

    err = queue.enqueueReleaseGLObjects(&_gl_objects, NULL, NULL);
    assert(err == CL_SUCCESS);
    queue.finish();
  }
  else
  {
    // We need to load the data explicitly into the texture
    kernel_call(*(this->_cl_buffer), _renderer->get_width(),
                _renderer->get_height());
    queue.finish();
    cl_int err;
    err = queue.enqueueReadImage(
        *_cl_buffer, CL_TRUE, {{0, 0, 0}},
        {{_renderer->get_width(), _renderer->get_height(), 1}}, 0, 0,
        _host_cl_buffer.data(), nullptr, nullptr);
    assert(err == CL_SUCCESS);
    queue.finish();

    // Copy Image to GL texture
    glBindTexture(GL_TEXTURE_2D, this->_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 static_cast<GLsizei>(_renderer->get_width()),
                 static_cast<GLsizei>(_renderer->get_height()), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, _host_cl_buffer.data());
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindTexture(GL_TEXTURE_2D, this->_texture);

  glEnable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glViewport(0, 0, static_cast<GLsizei>(_renderer->get_width()),
             static_cast<GLsizei>(_renderer->get_height()));

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(-1.0f, -1.0f);

  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(1.0f, -1.0f);

  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(1.0f, 1.0f);

  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(-1.0f, 1.0f);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glDisable(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);

  glGetError();
  // assert(glGetError() == GL_NO_ERROR);
}
