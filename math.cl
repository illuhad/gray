
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


