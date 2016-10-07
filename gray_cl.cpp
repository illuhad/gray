
#include <iostream>
#include "qcl.hpp"

#include "gl_renderer.hpp"
#include "cl_gl.hpp"
#include "scene.hpp"
#include "random.hpp"


int main(int argc, char* argv[])
{
  cl_gl::init_environment();
  gl_renderer::instance().init("gray", 1024, 1024, argc, argv);

  qcl::environment env;
  qcl::global_context_ptr ctx = env.create_global_gl_shared_context();

  if(ctx->get_num_devices() == 0)
  {
    std::cout << "No suitable OpenCL devices!" << std::endl;
    return -1;
  }

  ctx->global_register_source_file("pathtracer.cl",{"trace_paths"});
  qcl::kernel_ptr kernel = ctx->device(0)->get_kernel("trace_paths");
  
  std::string extensions;
  ctx->device(0)->get_supported_extensions(extensions);
  
  std::cout << "Supported extensions: " << extensions << std::endl;

  cl_gl cl_gl_interop(&(gl_renderer::instance()), ctx->device(0)->get_context());

  gray::device_object::random_engine random(ctx->device(0), 1024, 1024, 464545);

  auto kernel_execution = [&](const cl::ImageGL &pixels, std::size_t width, std::size_t height) 
  {
    cl::Event event;

    kernel->setArg(0, pixels);
    kernel->setArg(1, static_cast<int>(width));
    kernel->setArg(2, static_cast<int>(height));
    kernel->setArg(3, random.get());

    ctx->device(0)->get_command_queue().enqueueNDRangeKernel(*kernel,
                                                             cl::NullRange,
                                                             cl::NDRange(width, height),
                                                             cl::NDRange(8, 8),
                                                             NULL,
                                                             &event);

    event.wait();
  };

  gl_renderer::instance().on_display(
  [&]()
  {
    cl_gl_interop.display(kernel_execution, ctx->device(0)->get_command_queue());
  });

  gl_renderer::instance().render_loop();

  return 0;
}
