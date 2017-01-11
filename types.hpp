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


#ifndef TYPES_HPP
#define TYPES_HPP

#include <CL/cl2.hpp>

namespace gray {

using scalar = cl_float;
using vector3 = cl_float3;
using float4 = cl_float4;
using rgba_color = float4;
using rgb_color = cl_float3;

typedef cl_int portable_int;

inline static
rgba_color embed_rgb_in_rgba(const rgb_color& c, scalar alpha = 1.0f)
{
  return {{c.s[0], c.s[1], c.s[2], alpha}};
}
}

#endif
