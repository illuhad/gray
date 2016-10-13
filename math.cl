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

#ifndef MATH_HPP
#define MATH_HPP

#include "common_math.cl_hpp"

vector3 vec_from_to(vector3 from, vector3 to)
{
  vector3 result = to;
  result -= from;
  return result;
}

vector3 normalized_vec_from_to(vector3 from, vector3 to)
{
  return normalize(vec_from_to(from, to));
}

int vector_equals(vector3 a, vector3 b)
{
  return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
}


#endif


