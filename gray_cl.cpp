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

std::shared_ptr<gray::device_object::scene> 
setup_scene(const qcl::device_context_ptr& ctx)
{
  auto scene_ptr = 
    std::make_shared<gray::device_object::scene>(ctx, 2048, 1024);

  // Create materials
  auto material1 = scene_ptr->get_materials().allocate_material_map(1024, 1024);
  auto material2 = scene_ptr->get_materials().allocate_material_map(1024, 1024);
  auto material3 = scene_ptr->get_materials().allocate_material_map(1024, 1024);
  auto material4 = scene_ptr->get_materials().allocate_material_map(1024, 1024);
  auto emitting_material =
      scene_ptr->get_materials().allocate_material_map(1024, 1024);

  gray::material_factory material_fac{&(scene_ptr->get_materials())};

  material_fac.create_uniform_emissive_material(
    emitting_material,
    {{0.2f, 0.2f, 0.2f}});

  material_fac.create_uniform_emissive_material(
      scene_ptr->access_background_map(),
      {{0.1f, 0.08f, 0.15f}});

  material_fac.create_uniform_material(
      material1,
      {{1.0f, 1.0f, 1.0f}},
      1.0f, 1.6f, 1.e5f);
  
  material_fac.create_uniform_material(
      material2,
      {{0.2f, 0.6f, 0.2f}},
      0.0f, 1.0f, 10000.0f);

  material_fac.create_uniform_material(
      material3,
      {{0.8f, 0.2f, 0.2f}},
      0.0f, 1.0f, 8.0f);

  material_fac.create_uniform_material(
      material4,
      {{0.5f, 0.6f, 0.9f}},
      {{0.1f, 0.3f, 1.4f}},
      0.0f, 1.0f, 100.0f);

  scene_ptr->add_sphere({{0.0, -0.4, 0.0}}, 
                        {{0.0, 0.0, 1.0}}, 
                        {{1.0, 0.0, 1.0}},
                        1.0, material1);
  scene_ptr->add_sphere({{1.0, 1.5, 0.0}}, 
                        {{0.0, 0.0, 1.0}}, 
                        {{1.0, 0.0, 1.0}},
                        1.0, material4);

  scene_ptr->add_plane({{0.0, 0.0, -1.0}},
                       {{0.0, 0.0, 1.0}},
                       material2);

  scene_ptr->add_plane({{2.0, 0.0, 0.0}},
                       gray::math::normalize({{-1.0, 0.0, 1.0}}),
                       material3);
  // Use this for light emission
  /*
  scene_ptr->add_disk({{-2.0, 0.0, 2.0}},
                      {{1.0, 0.0, 0.0}},
                      10.0, emitting_material);
*/
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

int main(int argc, char* argv[])
{
  // Initialise interoperability environment
  cl_gl::init_environment();
  // Initialise render window
  gl_renderer::instance().init("gray", 1024, 1024, argc, argv);

  // Initialise OpenCL
  qcl::environment env;
  for(std::size_t i = 0; i < env.get_num_platforms(); ++i)
  {
    std::cout << "Platform " << i << ": " 
              << env.get_platform_name(env.get_platform(i))
              << " ["<< env.get_platform_vendor(env.get_platform(i)) <<"]"
              << std::endl;
  }


  qcl::global_context_ptr global_ctx = env.create_global_gl_shared_context();

  if(global_ctx->get_num_devices() == 0)
  {
    std::cout << "No suitable OpenCL devices!" << std::endl;
    return -1;
  }
  else
  {
    std::cout << "Found " << global_ctx->get_num_devices() << " device(s):" << std::endl;
    for(std::size_t i = 0; i < global_ctx->get_num_devices(); ++i)
      std::cout << "    Device " << i << ": " 
                << global_ctx->device(i)->get_device_name() << std::endl; 
  }
  // Compile sources and register kernels
  std::vector<std::string> kernels = {"trace_paths"};
  global_ctx->global_register_source_file("pathtracer.cl", {"trace_paths"});
  global_ctx->global_register_source_file("postprocessing.cl", {"hdr_color_compression"});
  global_ctx->global_register_source_file("reduction.cl", {"max_value_reduction_init", 
                                                          "max_value_reduction"});

  qcl::device_context_ptr ctx = global_ctx->device();

  std::string extensions;
  ctx->get_supported_extensions(extensions);
  
  std::cout << "Supported extensions: " << extensions << std::endl;
  
  // Create OpenGL <-> OpenCL interoperability
  auto cl_gl_interop = cl_gl{
    &(gl_renderer::instance()), ctx->get_context()
  };

  // Create scene
  auto scene = setup_scene(ctx);
  auto camera = setup_camera(ctx);

  // Create and launch rendering engine
  auto realtime_renderer = gray::realtime_window_renderer{
    ctx,
    &cl_gl_interop,
    scene.get(),
    camera.get()
  };

  realtime_renderer.get_render_engine().set_target_fps(40.0);

  gray::input_handler input;
  realtime_renderer.launch();
  gray::interactive_camera_control cam_controller(input, camera.get(), 
                                                  &realtime_renderer);
  gray::interactive_program_control program_controller(input);
  gl_renderer::instance().render_loop();

  return 0;
}
