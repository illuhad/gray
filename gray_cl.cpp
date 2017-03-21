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

#include "qcl.hpp"
#include <iostream>

#include "cl_gl.hpp"
#include "common_math.cl_hpp"
#include "gl_renderer.hpp"
#include "image.hpp"
#include "materials.hpp"
#include "realtime_renderer.hpp"
#include "scene.hpp"

std::shared_ptr<gray::device_object::scene>
setup_scene(const qcl::device_context_ptr& ctx)
{
  gray::image background{"skymap.hdr"};

  auto materials = std::make_shared<gray::device_object::material_db>(ctx);

  auto scene_ptr = std::make_shared<gray::device_object::scene>(
      ctx, materials, background.to_texture(*materials));

  gray::material_factory material_fac{&(scene_ptr->get_materials())};

  scene_ptr->add_sphere({{0.0, -0.4f, 0.1f}}, {{0.0, 0.0, 1.0}},
                        {{1.0, 0.0, 0.0}}, 1.1f,
                        material_fac.create_uniform_material(
                            {{0.9f, 1.0f, 1.0f}}, 1.0f, 1.6f, 1.e-5f));

  gray::texture_id inner_sphere_trs_texture =
      materials->allocate_texture(512, 512);
  gray::texture_accessor inner_sphere_trs_accessor =
      materials->access_texture(inner_sphere_trs_texture);

  for (std::size_t x = 0; x < inner_sphere_trs_accessor.get_width(); ++x)
  {
    for (std::size_t y = 0; y < inner_sphere_trs_accessor.get_height(); ++y)
    {
      gray::scalar y_normalized =
          static_cast<gray::scalar>(y) /
          static_cast<gray::scalar>(inner_sphere_trs_accessor.get_height());

      auto val = inner_sphere_trs_accessor.read(x, y);
      val.s[2] = std::sin(y_normalized * 8 * 2 * 3.145f) * 0.1f + 0.1f;

      inner_sphere_trs_accessor.write(val, x, y);
    }
  }

  gray::material_id inner_sphere_material = materials->create_material(
      material_fac.create_uniform_scattered_fraction_texture(
          {{0.5f, 0.8f, 0.14f}}),
      material_fac.create_uniform_emission_texture({{0.0f, 0.0f, 0.0f}}),
      inner_sphere_trs_texture);

  scene_ptr->add_sphere({{0.0, -0.4f, 0.1f}}, {{0.0, 0.0, 1.0}},
                        {{1.0, 0.0, 0.0}}, 0.3f, inner_sphere_material);

  gray::material_id diffuse = material_fac.create_uniform_material(
      {{0.8f, 0.8f, 0.8f}}, 0.0f, 1.0f, 0.8f);

  scene_ptr->add_sphere({{3.0, 1.0, -3.0}}, {{0.0, 0.0, 1.0}},
                        {{1.0, 0.0, 0.0}}, 3.0, diffuse);
  scene_ptr->add_sphere({{-3.0, 1.0, -3.0}}, {{0.0, 0.0, 1.0}},
                        {{1.0, 0.0, 0.0}}, 3.0, diffuse);

  scene_ptr->add_plane({{0.0, 0.0, -1.0}}, {{0.0, 0.0, 1.0}}, diffuse);

  scene_ptr->transfer_data();

  return scene_ptr;
}

std::shared_ptr<gray::device_object::camera>
setup_camera(const qcl::device_context_ptr& ctx)
{
  using namespace gray;

  vector3 camera_pos = {{0.0, -10.5, 2.0}};
  vector3 look_at = -1.f * camera_pos;
  look_at = math::normalize(look_at);

  auto camera_ptr = std::make_shared<gray::device_object::camera>(
      camera_pos, look_at,
      0.0,                                         // roll angle
      0.1f,                                        // aperture
      std::sqrt(math::dot(camera_pos, camera_pos)) // focal length
      );

  camera_ptr->enable_autofocus();

  return camera_ptr;
}

namespace gray {

class gray_app
{
public:
  gray_app(int argc, char** argv)
      : _x_resolution{1280}, _y_resolution{1024}, _rays_per_pixel{100},
        _argc{argc}, _argv{argv}
  {
    image::initialize(argc, argv);
  }

  void run()
  {
    _x_resolution = 1280;
    _y_resolution = 1024;
    _rays_per_pixel = 100;

    bool offline = false;
    bool disable_gl_sharing = false;

    std::vector<std::string> platform_preferences = {"NVIDIA", "AMD", "Intel"};

    if (_argc > 1)
    {
      for (std::size_t i = 1; i < static_cast<std::size_t>(_argc); ++i)
      {
        if (_argv[i] == std::string{"--offline"})
        {
          offline = true;
        }
        else if (_argv[i] == std::string{"--disable_gl_sharing"})
          disable_gl_sharing = true;
        else if (_argv[i] == std::string{"--prefer_platform"})
        {
          if (i == static_cast<std::size_t>(_argc) - 1)
            throw std::invalid_argument(
                "Invalid argument: Expected platform keyword.");

          std::string keyword = _argv[i + 1];
          platform_preferences.insert(platform_preferences.begin(), keyword);

          ++i;
        }
        else if (_argv[i] == std::string{"--resolution"})
        {
          if (i == static_cast<std::size_t>(_argc) - 1)
            throw std::invalid_argument(
                "Resolution not given after --resolution argument.");

          std::string resolution = _argv[i + 1];

          std::size_t resolution_delimiter = resolution.find("x");

          if (resolution_delimiter == std::string::npos)
            throw std::invalid_argument("Given resolution is invalid (expected "
                                        "format: x_resxy_res, e.g. 1024x1024)");

          std::string x_res = resolution.substr(0, resolution_delimiter);
          std::string y_res =
              resolution.substr(resolution_delimiter + 1, std::string::npos);

          this->_x_resolution = std::stoull(x_res);
          this->_y_resolution = std::stoull(y_res);

          ++i;
        }
        else if (_argv[i] == std::string{"--rays_per_pixel"})
        {
          if (i == static_cast<std::size_t>(_argc) - 1)
            throw std::invalid_argument("Number of rays per pixel not given "
                                        "after rays_per_pixel argument");

          _rays_per_pixel = std::stoull(_argv[i + 1]);

          ++i;
        }
        else
        {
          std::cout << "Invalid argument: " << _argv[i] << std::endl;
          return;
        }
      }
    }

    print_platforms(_environment);

    if (offline)
      launch_offline_renderer(platform_preferences);
    else
    {
      // Initialise interoperability environment
      cl_gl::init_environment();
      // Initialise render window
      gl_renderer::instance().init("gray", _x_resolution, _y_resolution, _argc,
                                   _argv);
      if (disable_gl_sharing)
        launch_realtime_renderer(platform_preferences, false);
      else
      {
        if (!launch_realtime_renderer(platform_preferences, true))
        {
          // The renderer could not be startet - try without gl sharing
          std::cout << "Could not start renderer - disabling OpenCL/OpenGL "
                       "object sharing and trying again..."
                    << std::endl;
          if (!launch_realtime_renderer(platform_preferences, false))
            throw std::runtime_error("Could not start fallback renderer.");
        }
      }
    }
  }

private:
  void print_platforms(const qcl::environment& env) const
  {
    for (std::size_t i = 0; i < env.get_num_platforms(); ++i)
    {
      std::cout << "Platform " << i << ": "
                << env.get_platform_name(env.get_platform(i)) << " ["
                << env.get_platform_vendor(env.get_platform(i)) << "]"
                << std::endl;
    }
  }

  void print_devices(const qcl::global_context_ptr& global_ctx) const
  {
    if (global_ctx->get_num_devices() == 0)
    {
      std::cout << "No suitable OpenCL devices!" << std::endl;
    }
    else
    {
      std::cout << "Found " << global_ctx->get_num_devices()
                << " device(s):" << std::endl;
      for (std::size_t i = 0; i < global_ctx->get_num_devices(); ++i)
        std::cout << "    Device " << i << ": "
                  << global_ctx->device(i)->get_device_name() << std::endl;
    }
  }

  void prepare_cl(qcl::global_context_ptr global_ctx) const
  {
    // Compile sources and register kernels
    global_ctx->global_register_source_file("pathtracer.cl", {"trace_paths"});
    global_ctx->global_register_source_file("postprocessing.cl",
                                            {"hdr_color_compression"});
    global_ctx->global_register_source_file(
        "reduction.cl", {"max_value_reduction_init", "max_value_reduction"});

    qcl::device_context_ptr ctx = global_ctx->device();

    std::string extensions;
    ctx->get_supported_extensions(extensions);

    std::cout << "Supported extensions: " << extensions << std::endl;
  }

  void launch_offline_renderer(
      const std::vector<std::string>& platform_preferences) const
  {

    const cl::Platform& selected_platform =
        _environment.get_platform_by_preference(platform_preferences);
    qcl::global_context_ptr global_ctx =
        _environment.create_global_context(selected_platform);

    print_devices(global_ctx);
    if (global_ctx->get_num_devices() == 0)
      throw std::runtime_error{"No devices found"};

    prepare_cl(global_ctx);
    qcl::device_context_ptr ctx = global_ctx->device();

    auto scene = setup_scene(ctx);
    auto camera = setup_camera(ctx);

    gray::frame_renderer renderer{ctx, "trace_paths", "hdr_color_compression",
                                  _x_resolution, _y_resolution};

    cl::Image2D pixels{ctx->get_context(), CL_MEM_READ_WRITE,
                       cl::ImageFormat{CL_RGBA, CL_UNORM_INT8}, _x_resolution,
                       _y_resolution};

    std::cout << "Started render..." << std::endl;

    // Each rendering chunk should take 2s
    renderer.set_target_rendering_time(2.0);
    while (renderer.get_total_rays_per_pixel() < _rays_per_pixel)
    {
      std::cout << "paths traced per pixel: "
                << renderer.get_total_rays_per_pixel() << std::endl;
      renderer.render(pixels, *scene, *camera);
    }

    ctx->get_command_queue().finish();

    std::cout << "Done." << std::endl;

    gray::image::save_png("gray_render.png", ctx, pixels, _x_resolution,
                          _y_resolution);
  }

  bool
  launch_realtime_renderer(const std::vector<std::string>& platform_preferences,
                           bool gl_sharing = true) const
  {
    // Initialise OpenCL
    qcl::global_context_ptr global_ctx;
    if (gl_sharing)
      global_ctx = _environment.create_global_gl_shared_context();
    else
    {
      const cl::Platform& selected_platform =
          _environment.get_platform_by_preference(platform_preferences);

      global_ctx = _environment.create_global_context(selected_platform);
    }

    print_devices(global_ctx);
    if (global_ctx->get_num_devices() > 0)
    {
      prepare_cl(global_ctx);

      qcl::device_context_ptr ctx = global_ctx->device();

      // Create OpenGL <-> OpenCL interoperability
      auto cl_gl_interop =
          cl_gl{&(gl_renderer::instance()), ctx->get_context(), gl_sharing};

      // Create scene
      auto scene = setup_scene(ctx);
      auto camera = setup_camera(ctx);

      // Create and launch rendering engine
      auto realtime_renderer = gray::realtime_window_renderer{
          ctx, &cl_gl_interop, scene.get(), camera.get()};

      realtime_renderer.get_render_engine().set_target_fps(20.0);

      gray::input_handler input;
      realtime_renderer.launch();
      gray::interactive_camera_control cam_controller(input, camera.get(),
                                                      &realtime_renderer);
      gray::interactive_program_control program_controller(input);
      gl_renderer::instance().render_loop();

      return true;
    }
    return false;
  }

  qcl::environment _environment;
  std::size_t _x_resolution;
  std::size_t _y_resolution;
  std::size_t _rays_per_pixel;
  int _argc;
  char** _argv;
};
}

int main(int argc, char* argv[])
{
  gray::gray_app app{argc, argv};
  app.run();

  return 0;
}
