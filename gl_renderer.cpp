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

#include <cassert>
#include <vector>
#include "gl_renderer.hpp"
#include "image.hpp"


void glut_display_func()
{ 
  gl_renderer::instance()._display_func();
  glutSwapBuffers();
  gl_renderer::instance().post_redisplay();
}

void glut_keyboard_func(unsigned char c, int x, int y)
{
  gl_renderer::instance()._keyboard_func(c,x,y);
  gl_renderer::instance().post_redisplay();
}

void glut_reshape_func(int width, int height)
{
  gl_renderer::instance()._width = width;
  gl_renderer::instance()._height = height;
  gl_renderer::instance()._reshape_func(width, height);

  gl_renderer::instance().post_redisplay();
}

void glut_mouse_func(int button, int state, int x, int y)
{
  gl_renderer::instance()._mouse_func(button, state, x, y);
  gl_renderer::instance().post_redisplay();
}
 
void glut_motion_func(int x, int y)
{
  gl_renderer::instance()._motion_func(x,y);
  gl_renderer::instance().post_redisplay();
}

void glut_idle_func()
{
  gl_renderer::instance()._idle_func();
  gl_renderer::instance().post_redisplay();
}

gl_renderer::gl_renderer()
:  _window_handle(0), _is_fullscreen(false)
{
  _display_func = []() {};
  _keyboard_func = [](unsigned char, int, int) {};
  _reshape_func = [](int, int) {};
  _mouse_func = [](int, int, int, int) {};
  _motion_func = [](int, int) {};
  _idle_func = []() {};
}

void gl_renderer::init(const std::string& title,
              std::size_t width, std::size_t height,
              int argc, char** argv)
{
  if(this->_window_handle)
    this->close();
  
  this->_width = static_cast<int>(width);
  this->_height = static_cast<int>(height);
  
  glutInit(&argc, argv);
  int screen_width = glutGet(GLUT_SCREEN_WIDTH);
  int screen_height = glutGet(GLUT_SCREEN_HEIGHT);
 
  glutInitDisplayMode( GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH );
 
  glutInitWindowPosition((screen_width - this->_width) / 2,
                         (screen_height - this->_height) / 2 );
  glutInitWindowSize(this->_width, this->_height);
 
  _window_handle = glutCreateWindow(title.c_str());
  
  glutDisplayFunc(glut_display_func);
  glutKeyboardFunc(glut_keyboard_func);
  glutIdleFunc(glut_idle_func);
  glutMouseFunc(glut_mouse_func);
  glutMotionFunc(glut_motion_func);
  glutReshapeFunc(glut_reshape_func);
 
  
  // Setup initial GL State
  glClearColor( 0.0f, 1.0f, 1.0f, 1.0f );
  glClearDepth( static_cast<GLclampd>(1.0f) );
 
  glShadeModel( GL_SMOOTH );
}

void gl_renderer::close()
{
  if(this->_window_handle)
  {
    glutDestroyWindow(this->_window_handle);
    this->_window_handle = 0;
  }
}

void gl_renderer::on_display(std::function<void ()> f)
{
  this->_display_func = f;
}

void gl_renderer::on_keyboard(std::function<void (unsigned char, int, int)> f)
{
  this->_keyboard_func = f;
}

void gl_renderer::on_reshape(std::function<void(int,int)> f)
{
  this->_reshape_func = f;
}

void gl_renderer::on_mouse(std::function<void(int,int,int,int)> f)
{
  this->_mouse_func = f;
}

void gl_renderer::on_motion(std::function<void(int,int)> f)
{
  this->_motion_func = f;
}

void gl_renderer::on_idle(std::function<void()> f)
{
  this->_idle_func = f;
}

void gl_renderer::render_loop()
{
  glutMainLoop();
}

void gl_renderer::toggle_fullscreen(bool fullscreen)
{
  if(fullscreen != _is_fullscreen)
  {
    if(fullscreen)
      glutFullScreen();
    else
    {
      glutPositionWindow(20,20);
      glutReshapeWindow(1200, 900);
    }
    
    _is_fullscreen = fullscreen;
  }
}

bool gl_renderer::is_fullscreen() const
{
  return _is_fullscreen;
}

void gl_renderer::save_png_screenshot(const std::string& name) const
{
  std::size_t num_bytes = 3 * static_cast<std::size_t>(_width) 
                            * static_cast<std::size_t>(_height);
      
  std::vector<unsigned char> buffer(num_bytes);
      
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glReadPixels(0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, buffer.data());

  save_png(name, buffer,
           static_cast<std::size_t>(_width), static_cast<std::size_t>(_height));
}

void gl_renderer::post_redisplay()
{
  if(this->_window_handle)
    glutPostRedisplay();
}

gl_renderer::~gl_renderer()
{
}

