
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
  auto emitting_material = 
                   scene_ptr->get_materials().allocate_material_map(1024, 1024);

  gray::material_factory material_fac{&(scene_ptr->get_materials())};

  material_fac.create_uniform_emissive_material(
    emitting_material,
    {{1.0f, 1.0f, 1.0f}});

  material_fac.create_uniform_emissive_material(
      scene_ptr->access_background_map(),
      {{0.04f, 0.06f, 0.1f}});

  material_fac.create_uniform_material(
      material1,
      {{0.5f, 0.6f, 0.8f}},
      0.0f, 1.0f, 1.0f);
  
  material_fac.create_uniform_material(
      material2,
      {{0.5f, 0.8f, 0.2f}},
      0.0f, 1.0f, 3.0f);
  
  scene_ptr->add_sphere({{0.0, 0.0, 0.0}}, 
                        {{0.0, 0.0, 1.0}}, 
                        {{1.0, 0.0, 0.0}},
                        1.0, material1);
  scene_ptr->add_plane({{0.0, 0.0, -1.0}},
                       {{0.0, 0.0, 1.0}},
                       material2);
  // Use this for light emission
  scene_ptr->add_disk({{-3.0, 0.0, 2.0}},
                      {{1.0, 0.0, 0.0}},
                      2.0, emitting_material);

  scene_ptr->transfer_data();

  return scene_ptr;
}

std::shared_ptr<gray::device_object::camera>
setup_camera(const qcl::device_context_ptr& ctx)
{
  using namespace gray;

  vector3 camera_pos = {{0.0, -3.0, 2.0}};
  vector3 look_at = -1.f * camera_pos;
  look_at = math::normalize(look_at);
  
  auto camera_ptr =
      std::make_shared<gray::device_object::camera>(
          camera_pos,
          look_at,
          0.0, // roll angle
          1.0, // aperture
          3.0  // focal length
          );

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
  qcl::global_context_ptr global_ctx = env.create_global_gl_shared_context();

  if(global_ctx->get_num_devices() == 0)
  {
    std::cout << "No suitable OpenCL devices!" << std::endl;
    return -1;
  }
  // Compile sources
  global_ctx->global_register_source_file("pathtracer.cl",{"trace_paths"});
  
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

  realtime_renderer.launch();
  gl_renderer::instance().render_loop();

  return 0;
}
