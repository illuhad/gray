
#ifndef TYPES_HPP
#define TYPES_HPP

#include <CL/cl.hpp>

namespace gray {

using scalar = cl_float;
using vector3 = cl_float3;
using float4 = cl_float4;
using rgba_color = float4;
using rgb_color = cl_float3;

rgba_color embed_rgb_in_rgba(const rgb_color& c, scalar alpha = 1.0f)
{
  return {{c.s[0], c.s[1], c.s[2], alpha}};
}
}

#endif
