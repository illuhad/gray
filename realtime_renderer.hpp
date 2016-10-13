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

#ifndef REALTIME_RENDERER_HPP
#define REALTIME_RENDERER_HPP

#include "frame_renderer.hpp"
#include "gl_renderer.hpp"
#include "cl_gl.hpp"

#include <map>


namespace gray {


class realtime_window_renderer
{
public:
  realtime_window_renderer(const qcl::device_context_ptr& ctx,
                         cl_gl* cl_gl_interoperability,
                         const device_object::scene* s,
                         const device_object::camera* cam)
  : _cl_gl_interoperability{cl_gl_interoperability},
    _scene{s}, _camera{cam},
    _renderer
    {
      ctx, 
      "trace_paths",
      "hdr_color_compression", 
      gl_renderer::instance().get_width(),
      gl_renderer::instance().get_height()
    }
  {
    assert(s);
    assert(cam);
  }

  void launch()
  {
    gl_renderer::instance().on_display([this]() {
      display();
    });

    gl_renderer::instance().on_reshape([this](int width, int height) {
      update_resolution(width, height);
    });
  }

  const frame_renderer& get_render_engine() const
  {
    return _renderer;
  }

  frame_renderer& get_render_engine()
  {
    return _renderer;
  }

private:

  void display()
  {
    auto rendering_call =
        [this](const cl::ImageGL &pixels, std::size_t width, std::size_t height) {

          if (_renderer.get_resolution_width() == width ||
              _renderer.get_resolution_height() == height)
          {
            _renderer.render(pixels, *_scene, *_camera);
          }
        };

    _cl_gl_interoperability->display(rendering_call,
                                     _renderer.get_current_context()->get_command_queue());

  }

  void update_resolution(std::size_t width, std::size_t height)
  {
    _renderer.get_cl_context()->get_command_queue().finish();
    _cl_gl_interoperability->rebuild_buffers();
    _renderer.set_resolution(width, height);
    _renderer.get_cl_context()->get_command_queue().finish();
  }

  cl_gl *_cl_gl_interoperability;

  const device_object::scene* _scene;
  const device_object::camera *_camera;

  frame_renderer _renderer;

};

class input_handler
{
public:
  input_handler()
  : _is_lmb_down{false}, _is_rmb_down{false},
    _prev_mouse_pos_x{-1}, _prev_mouse_pos_y{-1}
  {
    gl_renderer::instance().on_keyboard([this](unsigned char c, int x, int y) {
      this->handle_key(c, x, y);
    });
    gl_renderer::instance().on_mouse([this](int button, int state, int x, int y) {
      if(button == GLUT_LEFT_BUTTON || button == GLUT_RIGHT_BUTTON)
      {
        if (button == GLUT_LEFT_BUTTON)
        {
          if (state == GLUT_DOWN)
            _is_lmb_down = true;
          else
            _is_lmb_down = false;
        }
        else if (button == GLUT_RIGHT_BUTTON)
        {
          if (state == GLUT_DOWN)
            _is_rmb_down = true;
          else
            _is_rmb_down = false;
        }
        for(auto& handler : _mouse_handlers)
          handler(this, button, state, x, y);
      }
      else if(button == 3 || button == 4)
      {
        // wheel events
        int wheel = 0;
        int direction = 0;

        if (button == 3)
          direction = 1;
        else direction = -1;

        for (auto &handler : _wheel_handlers)
          handler(this, wheel, direction, x, y);
      } 
    });

    gl_renderer::instance().on_motion([this](int x, int y) {

      int delta_x = 0;
      int delta_y = 0;

      if(_prev_mouse_pos_x != -1)
        delta_x = x - _prev_mouse_pos_x;
      if(_prev_mouse_pos_y != -1)
        delta_y = y - _prev_mouse_pos_y;

      _prev_mouse_pos_x = x;
      _prev_mouse_pos_y = y;

      for(auto& handler : _motion_handlers)
        handler(this, x, y, delta_x, delta_y);
    });
  }

  input_handler(const input_handler &other) = delete;
  input_handler &operator=(const input_handler &other) = delete;

  using key_pressed_handler = std::function<void(input_handler*, int, int)>;
  using mouse_handler = std::function<void(input_handler *, int, int, int, int)>;
  using mouse_wheel_handler = std::function<void(input_handler *, int, int, int, int)>;
  using mouse_motion_handler = std::function<void(input_handler *, int, int, int, int)>;


  void add_key_event(unsigned char c, key_pressed_handler handler)
  {
    _key_events.insert(std::make_pair(c, handler));
  }

  void add_mouse_event(mouse_handler handler)
  {
    _mouse_handlers.push_back(handler);
  }

  void add_mouse_wheel_event(mouse_wheel_handler handler)
  {
    _wheel_handlers.push_back(handler);
  }

  void add_mouse_motion_event(mouse_motion_handler handler)
  {
    _motion_handlers.push_back(handler);
  }

  inline bool is_left_mouse_down() const
  {
    return _is_lmb_down;
  }

  inline bool is_right_mouse_down() const
  {
    return _is_rmb_down;
  }

private:
  void handle_key(unsigned char c, int x, int y)
  {
    auto range = _key_events.equal_range(c);
    for (auto it = range.first; it != range.second; ++it)
      (it->second)(this, x, y);
  }

  std::multimap<unsigned char, key_pressed_handler> _key_events;

  bool _is_lmb_down;
  bool _is_rmb_down;

  std::vector<mouse_handler> _mouse_handlers;
  std::vector<mouse_wheel_handler> _wheel_handlers;
  std::vector<mouse_motion_handler> _motion_handlers;

  int _prev_mouse_pos_x;
  int _prev_mouse_pos_y;
};

class interactive_program_control
{
public:
  interactive_program_control(input_handler& input)
  {
    input.add_key_event('f', [this](input_handler *input, int x, int y) {
      gl_renderer::instance().toggle_fullscreen(!gl_renderer::instance().is_fullscreen());
    });

    input.add_key_event('q', [this](input_handler *input, int x, int y) {
      gl_renderer::instance().close();
    });
  }
};

class interactive_camera_control
{
public:
  interactive_camera_control(input_handler& input,
                            device_object::camera* cam,
                            realtime_window_renderer* realtime_renderer)
  : _camera{cam}, _renderer{realtime_renderer}, _last_camera_focus{1.0}
  {
    assert(cam);
    assert(realtime_renderer);

    input.add_key_event('w', [this](input_handler *input, int x, int y) {
      vector3 new_pos = _camera->get_position() 
                      + get_movement_amount() * _camera->get_look_at();
      _camera->set_position(new_pos);
      _renderer->get_render_engine().discard_render_results();
    });

    input.add_key_event('s', [this](input_handler *input, int x, int y) {
      vector3 new_pos = _camera->get_position() 
                      -  get_movement_amount() * _camera->get_look_at();
      _camera->set_position(new_pos);
      _renderer->get_render_engine().discard_render_results();
    });

    input.add_key_event('a', [this](input_handler *input, int x, int y) {
      vector3 new_pos = _camera->get_position() 
                      + get_movement_amount() * _camera->get_screen_basis1();
      _camera->set_position(new_pos);
      _renderer->get_render_engine().discard_render_results();
    });


    input.add_key_event('d', [this](input_handler *input, int x, int y) {
      vector3 new_pos = _camera->get_position() 
                      - get_movement_amount() * _camera->get_screen_basis1();
      _camera->set_position(new_pos);
      _renderer->get_render_engine().discard_render_results();
      
    });

    input.add_key_event('z', [this](input_handler *input, int x, int y) {
      if(!_camera->is_autofocus_enabled())
      {
        _last_camera_focus = _camera->get_focal_length();
        _camera->enable_autofocus();
      }
      else
        _camera->set_focal_length(_last_camera_focus);

    });

    input.add_key_event('p', [this](input_handler *input, int x, int y) {
      // Screenshot
      gl_renderer::instance().save_png_screenshot("gray_render.png");
    });

    input.add_mouse_wheel_event([this](input_handler *input, int wheel, int direction, int x, int y) {
      // scrolling - change focal distance
      if(_camera->is_autofocus_enabled())
      {
        _camera->set_focal_length(_last_camera_focus);
        return;
      }

      scalar focal_plane = _camera->get_focal_plane_distance();
      focal_plane += direction * get_movement_amount();

      if (focal_plane < 1.e-4f)
        focal_plane = 1.e-4f;
      
      _camera->set_focal_plane_distance(focal_plane);

      _renderer->get_render_engine().discard_render_results();
    });

    input.add_mouse_motion_event([this](input_handler *input, int x, int y,
                                        int delta_x, int delta_y) {
      if (input->is_left_mouse_down())
      {
        math::matrix3x3 rotation_matrix;
        vector3 new_look_at;

        math::matrix_create_rotation_matrix(&rotation_matrix, 
                                            _camera->get_screen_basis1(), 
                                            delta_y * get_angular_movement_amount());
        new_look_at = math::matrix_vector_mult(&rotation_matrix, _camera->get_look_at());
       
        math::matrix_create_rotation_matrix(&rotation_matrix, 
                                            _camera->get_screen_basis2(), 
                                            -delta_x * get_angular_movement_amount());
        new_look_at = math::matrix_vector_mult(&rotation_matrix, new_look_at);

        _camera->set_look_at(new_look_at);
        _renderer->get_render_engine().discard_render_results();
      }
    });
  }

private:
  static constexpr scalar movement_per_sec = 3.0f;
  static constexpr scalar angular_movement_per_sec = 0.05f;

  inline
  scalar get_movement_amount() const
  {
    return movement_per_sec * 1.f / _renderer->get_render_engine().get_current_fps();
  }

  inline
  scalar get_angular_movement_amount() const
  {
    return angular_movement_per_sec * 1.f / _renderer->get_render_engine().get_current_fps();
  }

  device_object::camera* _camera;
  realtime_window_renderer *_renderer;

  scalar _last_camera_focus;
};
}

#endif
