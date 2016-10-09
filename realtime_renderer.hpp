#ifndef REALTIME_RENDERER_HPP
#define REALTIME_RENDERER_HPP

#include "frame_renderer.hpp"
#include "gl_renderer.hpp"
#include "cl_gl.hpp"

namespace gray {


class realtime_window_renderer
{
public:
  realtime_window_renderer(const qcl::device_context_ptr& ctx,
                         cl_gl* cl_gl_interoperability,
                         const device_object::scene* s,
                         const device_object::camera* cam)
  : _scene{s}, _camera{cam},
    _renderer
    {
      ctx, 
      "trace_paths", 
      gl_renderer::instance().get_width(),
      gl_renderer::instance().get_height()
    }
  {
    assert(s);
    assert(cam);
  }

  void launch()
  {
    gl_renderer::instance().on_display([this]() { display(); });
    gl_renderer::instance().on_reshape([this](int width, int height) {
      if (_renderer.get_resolution_width() != static_cast<std::size_t>(width) ||
          _renderer.get_resolution_height() != static_cast<std::size_t>(height))
      {
        update_resolution(static_cast<std::size_t>(width),
                          static_cast<std::size_t>(height));
      }
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
            // Only render if we haven't changed resolution. This ensures
            // that we are not using pixels which may have become invalid
            // during the resolution change in case display() is called
            // before the reshape event.
            _renderer.render(pixels, *_scene, *_camera);
          }
        };

    _cl_gl_interoperability->display(rendering_call,
                                     _renderer.get_current_context()->get_command_queue());

  }

  void update_resolution(std::size_t width, std::size_t height)
  {
    _renderer.set_resolution(width, height);
    _cl_gl_interoperability->rebuild_buffers();
  }

  cl_gl *_cl_gl_interoperability;

  const device_object::scene* _scene;
  const device_object::camera *_camera;

  frame_renderer _renderer;
};
}

#endif
