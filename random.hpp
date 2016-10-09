
#ifndef RANDOM_HPP
#define RANDOM_HPP

#include "qcl.hpp"
#include <random>

namespace gray {
namespace device_object {


class random_engine
{
public:
  static std::size_t generate_seed()
  {
    std::random_device rd;
    return rd();
  }

  random_engine(const qcl::device_context_ptr& ctx,
                   std::size_t width, std::size_t height, 
                   std::size_t seed)
  : _ctx{ctx}, _gen{seed}
  {
    init(width, height);
  }

  void resize(std::size_t width, std::size_t height)
  {
    init(width, height);
  }

  const cl::Buffer& get() const
  {
    return _state;
  }
  
  cl::Buffer& get()
  {
    return _state;
  }
  
private:
  void init(std::size_t width, std::size_t height)
  {
    std::vector<cl_int> random_init(width * height);
    for(std::size_t i = 0; i < random_init.size(); ++i)
    {
      std::mt19937::result_type rand_number = _gen();
      random_init[i] = static_cast<int>(rand_number);
    }
    _ctx->create_buffer<cl_int>(_state, CL_MEM_READ_WRITE, width*height, random_init.data());
    _ctx->memcpy_h2d<cl_int>(_state, random_init.data(), random_init.size());
  }

  qcl::const_device_context_ptr _ctx;
  std::mt19937 _gen;

  cl::Buffer _state;
};

}
}

#endif
