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


#include <iostream>
#include "qcl.hpp"

#include "gl_renderer.hpp"
#include "cl_gl.hpp"
#include "realtime_renderer.hpp"
#include "materials.hpp"
#include "common_math.cl_hpp"
#include "image.hpp"

std::shared_ptr<gray::device_object::scene> 
setup_scene(const qcl::device_context_ptr& ctx)
{
  gray::image background{"skymap.hdr"};

  auto scene_ptr = 
    std::make_shared<gray::device_object::scene>(ctx, background.get_width(), 
                                                      background.get_height());

  gray::material_factory material_fac{&(scene_ptr->get_materials())};
  material_fac.create_uniform_emissive_material(
      scene_ptr->access_background_map(),
      {{0.0f, 0.0f, 0.0f}});
  gray::texture background_emissive_texture = scene_ptr->access_background_map().get_emitted_light();
  background.write_texture(background_emissive_texture);

  // Create materials
  auto material1 = scene_ptr->get_materials().allocate_material_map(1, 1);
  auto material2 = scene_ptr->get_materials().allocate_material_map(1, 1);
  auto diffuse = scene_ptr->get_materials().allocate_material_map(1, 1);
  auto material4 = scene_ptr->get_materials().allocate_material_map(1, 1);
  auto material5 = scene_ptr->get_materials().allocate_material_map(512, 512);
  auto mirror_material =
      scene_ptr->get_materials().allocate_material_map(1, 1);
  auto emissive_material =
      scene_ptr->get_materials().allocate_material_map(1, 1);
  auto medium_material =
      scene_ptr->get_materials().allocate_material_map(1, 1);


  material_fac.create_uniform_material(
    mirror_material,
    {{0.9f, 0.76f, 0.8f}},
    0.0f, 1.0f, 1.e-5f);

  material_fac.create_uniform_material(
    medium_material,
    {{1.0f, 1.0f, 1.0f}},
    1.0f, 1.0f, 1.e-5);

  material_fac.create_uniform_emissive_material(
      emissive_material,
      {{1.2f, 1.2f, 1.2f}});

  material_fac.create_uniform_material(
      material1,
      {{0.9f, 1.0f, 1.0f}},
      1.0f, 1.6f, 1.e-5f);
  
  material_fac.create_uniform_material(
      material2,
      {{0.2f, 0.4f, 0.2f}},
      0.0f, 1.0f, 1.0f);

  material_fac.create_uniform_material(
      diffuse,
      {{0.8f, 0.8f, 0.8f}},
      0.0f, 1.0f, 0.8f);

  material_fac.create_uniform_material(
      material4,
      {{0.5f, 0.6f, 0.9f}},
      {{0.1f, 0.3f, 1.4f}},
      0.0f, 1.0f, 1.0f);

  material_fac.create_uniform_material(
      material5,
      {{0.5f, 0.8f, 0.14f}},
      //{{0.5f, 0.8f, 0.14f}},
      0.0f, 1.0f, 0.8f);

  gray::texture material5_trs = 
    scene_ptr->get_materials().get_material_map(material5).get_transmittance_refraction_roughness();
  gray::texture material5_scattered =
      scene_ptr->get_materials().get_material_map(material5).get_scattered_fraction();
  gray::texture emitted_mat_tex = 
      scene_ptr->get_materials().get_material_map(material4).get_emitted_light();

  for (std::size_t x = 0; x < material5_trs.get_width(); ++x)
    for(std::size_t y = 0; y < material5_trs.get_height(); ++y)
    {
      gray::scalar x_normalized = static_cast<gray::scalar>(x) /
                            static_cast<gray::scalar>(material5_scattered.get_width());
      gray::scalar y_normalized = static_cast<gray::scalar>(y) /
                            static_cast<gray::scalar>(material5_trs.get_height());

      auto val = material5_trs.read(x,y);
      val.s[2] = std::sin(y_normalized * 8 * 2 * 3.145f) *
                     0.1f +
                 0.1f;

      material5_trs.write(val, x, y);

      val = material5_scattered.read(x, y);
      //val.s[0] = 0.5f * std::cos(10 * M_PI * x_normalized) + 0.5f;
      material5_scattered.write(val, x, y);

    }

  scene_ptr->add_sphere({{0.0, -0.4, 0.1}},
                          {{0.0, 0.0, 1.0}},
                          {{1.0, 0.0, 0.0}},
                          1.1, material1);
  /*scene_ptr->add_sphere({{1.0, 1.5, 61.0}}, 
                        {{0.0, 1.0, 0.0}}, 
                        {{1.0, 0.0, 0.0}},
                        55.0, material4);*/
  scene_ptr->add_sphere({{0.0, -0.4, 0.1}}, 
                        {{0.0, 0.0, 1.0}}, 
                        {{1.0, 0.0, 0.0}},
                        0.3, material5);   
  /*scene_ptr->add_sphere({{0.0, -0.4, 0.1}}, 
                        {{0.0, 0.0, 1.0}}, 
                        {{1.0, 0.0, 0.0}},
                        0.8, medium_material); */                      

  scene_ptr->add_sphere({{3.0, 1.0, -3.0}}, 
                        {{0.0, 0.0, 1.0}}, 
                        {{1.0, 0.0, 0.0}},
                        3.0, diffuse);
  scene_ptr->add_sphere({{-3.0, 1.0, -3.0}}, 
                        {{0.0, 0.0, 1.0}}, 
                        {{1.0, 0.0, 0.0}},
                        3.0, diffuse);                         

  scene_ptr->add_plane({{0.0, 0.0, -1.0}},
                       {{0.0, 0.0, 1.0}},
                       diffuse);
/*
  scene_ptr->add_disk({{4.0, 0.0, -1.0}},
                       gray::math::normalize({{-1.0, 0.0, 1}}), 20.0,
                       mirror_material);
  scene_ptr->add_disk({{-4.0, 0.0, -1.0}},
                      gray::math::normalize({{1.0, 0.0, 1}}), 20.0,
                      mirror_material);
  scene_ptr->add_disk({{0.0, 7.0, 0.0}},
                      {{0.0, 1.0, 0.0}}, 20.0,
                      mirror_material);                    
  scene_ptr->add_disk({{0.0, -15.0, 0.0}},
                      {{0.0, 1.0, 0.0}}, 20.0,
                      mirror_material);

  scene_ptr->add_disk({{0.0, 6.0, 3.0}},
                      gray::math::normalize({{0.0, -6, -3}}), 0.5,
                      emissive_material);    */
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
  
  auto camera_ptr =
      std::make_shared<gray::device_object::camera>(
          camera_pos,
          look_at,
          0.0, // roll angle
          0.1f, // aperture
          std::sqrt(math::dot(camera_pos, camera_pos))  // focal length
          );

  camera_ptr->enable_autofocus();

  return camera_ptr;
}


namespace gray {

class gray_app
{
public:
  gray_app(int argc, char** argv)
  {
    image::initialize(argc, argv);
    
    bool offline = false;
    bool disable_gl_sharing = false;

    std::vector<std::string> platform_preferences = {"NVIDIA", "AMD", "Intel"};

    if (argc > 1)
    {
      for(std::size_t i = 1; i < static_cast<std::size_t>(argc); ++i)
      {
        if(argv[i] == std::string{"--offline"})
          offline = true;
        else if(argv[i] == std::string{"--disable_gl_sharing"})
          disable_gl_sharing = true;
        else if(argv[i] == std::string{"--prefer_platform"})
        {
          if(i == static_cast<std::size_t>(argc) - 1)
            throw std::invalid_argument("Invalid argument: Expected platform keyword.");

          std::string keyword = argv[i + 1];
          platform_preferences.insert(platform_preferences.begin(), keyword);

          ++i;
        }
      }
    }
    
    print_platforms(_environment);

    if(offline)
      launch_offline_renderer(platform_preferences);
    else
    {
      // Initialise interoperability environment
      cl_gl::init_environment();
      // Initialise render window
      gl_renderer::instance().init("gray", 1920, 1080, argc, argv);
      if(disable_gl_sharing)
        launch_realtime_renderer(platform_preferences, false);
      else
      {
        if(!launch_realtime_renderer(platform_preferences, true))
        {
          // The renderer could not be startet - try without gl sharing
          std::cout << "Could not start renderer - disabling OpenCL/OpenGL object sharing and trying again..."
                    << std::endl;
          if(!launch_realtime_renderer(platform_preferences, false))
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
                << env.get_platform_name(env.get_platform(i))
                << " [" << env.get_platform_vendor(env.get_platform(i)) << "]"
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
      std::cout << "Found " << global_ctx->get_num_devices() << " device(s):" << std::endl;
      for (std::size_t i = 0; i < global_ctx->get_num_devices(); ++i)
        std::cout << "    Device " << i << ": "
                  << global_ctx->device(i)->get_device_name() << std::endl;
    }
  }

  void prepare_cl(qcl::global_context_ptr global_ctx) const
  {
    // Compile sources and register kernels
    global_ctx->global_register_source_file("pathtracer.cl", {"trace_paths"});
    global_ctx->global_register_source_file("postprocessing.cl", {"hdr_color_compression"});
    global_ctx->global_register_source_file("reduction.cl", {"max_value_reduction_init",
                                                             "max_value_reduction"});

    qcl::device_context_ptr ctx = global_ctx->device();

    std::string extensions;
    ctx->get_supported_extensions(extensions);

    std::cout << "Supported extensions: " << extensions << std::endl;
  }


  void launch_offline_renderer(const std::vector<std::string>& platform_preferences) const
  {
    const cl::Platform &selected_platform = _environment.get_platform_by_preference(platform_preferences);
    qcl::global_context_ptr global_ctx = _environment.create_global_context(selected_platform);

    print_devices(global_ctx);
    if(global_ctx->get_num_devices()==0)
      throw std::runtime_error{"No devices found"};
      
    prepare_cl(global_ctx);
    qcl::device_context_ptr ctx = global_ctx->device();

    auto scene = setup_scene(ctx);
    auto camera = setup_camera(ctx);
  }
  

  bool launch_realtime_renderer(const std::vector<std::string>& platform_preferences, bool gl_sharing = true) const
  {

    // Initialise OpenCL
    qcl::global_context_ptr global_ctx;
    if(gl_sharing)
      global_ctx = _environment.create_global_gl_shared_context();
    else
    {
      const cl::Platform &selected_platform = _environment.get_platform_by_preference(platform_preferences);

      global_ctx = _environment.create_global_context(selected_platform);
    }

    print_devices(global_ctx);
    if (global_ctx->get_num_devices() > 0)
    {
      prepare_cl(global_ctx);

      qcl::device_context_ptr ctx = global_ctx->device();

      // Create OpenGL <-> OpenCL interoperability
      auto cl_gl_interop = cl_gl{
          &(gl_renderer::instance()), ctx->get_context(), gl_sharing};

      // Create scene
      auto scene = setup_scene(ctx);
      auto camera = setup_camera(ctx);

      // Create and launch rendering engine
      auto realtime_renderer = gray::realtime_window_renderer{
          ctx,
          &cl_gl_interop,
          scene.get(),
          camera.get()};

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
};
}

int main(int argc, char* argv[])
{
  gray::gray_app app{argc, argv};

  return 0;
}
