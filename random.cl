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

#ifndef RANDOM_CL
#define RANDOM_CL

#include "math.cl"

//typedef __local int random_ctx;
typedef struct
{
  __global int *state_buffer;
  int local_state;
} random_ctx;

__constant const unsigned random_max = 2147483647;
__constant const unsigned long random_a = 16807;



void random_init(random_ctx* ctx, __global int* state_buffer)
{
  int global_id = get_global_id(0) * get_global_size(1) + get_global_id(1);

  ctx->state_buffer = state_buffer;
  ctx->local_state = state_buffer[global_id];
}

void random_fini(random_ctx* ctx)
{
  int global_id = get_global_id(0) * get_global_size(1) + get_global_id(1);

  ctx->state_buffer[global_id] = ctx->local_state;
}

int random_serial_int(int* state)
{
  int s = *state;
  s = ((unsigned long)(s) * random_a) % random_max;
  *state = s;
  
  return s;
}

scalar random_serial_scalar(int* state)
{
  return (scalar)random_serial_int(state) / (scalar)random_max;
}

scalar random_uniform_scalar(random_ctx* ctx)
{
  scalar random_number = random_serial_scalar(&(ctx->local_state));
 
  return random_number;
}

scalar random_uniform_scalar_max(random_ctx* ctx, const scalar max)
{
  return max * random_uniform_scalar(ctx);
}

scalar random_uniform_scalar_minmax(random_ctx* ctx, const scalar min, const scalar max)
{
  return (max - min) * random_uniform_scalar(ctx) + min;
}

int random_uniform_int(random_ctx* ctx)
{
  int random_number = random_serial_int(&(ctx->local_state));
  
  return random_number;
}

vector3 random_uniform_sphere(random_ctx* ctx)
{
  /*
  scalar x1, x2;
  scalar r;
  do
  {
    x1 = 2.f * random_uniform_scalar(ctx) - 1.f;
    x2 = 2.f * random_uniform_scalar(ctx) - 1.f;
    r = x1 * x1;
    r += x2 * x2;
  } while(r >= 1.f);
  
  scalar sqrt_term = sqrt(1.f - r);
  
  vector3 result;
  result.x = 2 * x1 * sqrt_term;
  result.y = 2 * x2 * sqrt_term;
  result.z = 1.f - 2 * r; */

  scalar u = random_uniform_scalar_minmax(ctx, -1.f, 1.f);
  scalar phi = random_uniform_scalar_minmax(ctx, 0, 2.f * M_PI_F);
  scalar sqrt_term = sqrt(1.f - u);
  scalar cos_phi = cos(phi);
  vector3 result;
  result.x = sqrt_term * cos_phi;
  result.y = sqrt_term * sin(phi);
  result.z = u;

  return result;
}

vector3 random_power_cosine(random_ctx* ctx, vector3 distribution_center, scalar power)
{
  vector3 random_point_on_sphere = random_uniform_sphere(ctx);
  scalar lambda = -dot(distribution_center, random_point_on_sphere);
  
  vector3 second_basis_vector = random_point_on_sphere;
  second_basis_vector += lambda * distribution_center;
  second_basis_vector = normalize(second_basis_vector);
  
  scalar cos_theta = pow(random_uniform_scalar(ctx), 1.f / (power + 1.f));
  scalar sin_theta = sqrt(1.f - cos_theta * cos_theta);
  
  vector3 result = cos_theta * distribution_center;
  result += sin_theta * second_basis_vector;
  
  return result;
}

vector3 random_cosine(random_ctx* ctx, vector3 distribution_center)
{
  vector3 random_point_on_sphere = random_uniform_sphere(ctx);
  scalar lambda = -dot(distribution_center, random_point_on_sphere);
  
  vector3 second_basis_vector = random_point_on_sphere;
  second_basis_vector += lambda * distribution_center;
  second_basis_vector = normalize(second_basis_vector);
  
  scalar cos_theta = random_uniform_scalar(ctx);
  scalar sin_theta = sqrt(1.f - cos_theta * cos_theta);
  
  vector3 result = cos_theta * distribution_center;
  result += sin_theta * second_basis_vector;
  
  return result;
}

vector3 random_point_on_cone(random_ctx* ctx, vector3 distribution_center, scalar cos_theta)
{
  vector3 first_basis_vector = distribution_center;
  // Make sure vector is different than the basis vector
  first_basis_vector.x += 1.0f;
  first_basis_vector = normalize(cross(distribution_center, first_basis_vector));

  vector3 second_basis_vector = cross(first_basis_vector, distribution_center);

  scalar phi = random_uniform_scalar_minmax(ctx, 0.0f, 2.0f * M_PI_F);
  scalar sqrt_term = sqrt(1.f - cos_theta * cos_theta);

  vector3 result = sqrt_term * cos(phi) * first_basis_vector;
  result += sqrt_term * sin(phi) * second_basis_vector;
  result += cos_theta * distribution_center;

  return result;
}

scalar random_ggx_cos_theta(random_ctx* ctx, scalar roughness)
{
  scalar random_scalar = random_uniform_scalar(ctx);
  //return atan(roughness * sqrt(random_scalar / (1.f - random_scalar)));
  return sqrt((1.f - random_scalar) / (random_scalar * (roughness * roughness - 1.f) + 1.f));
}

scalar random_beckmann_cos_theta(random_ctx* ctx, scalar roughness)
{
  scalar random_scalar = random_uniform_scalar(ctx);
  //return atan(-roughness * roughness * log(1.f - random_scalar));
  return rsqrt(1.f - roughness * roughness * log(1.f - random_scalar));
}

vector3 random_isotropic_ggx(random_ctx* ctx, vector3 distribution_center, scalar roughness)
{
  return random_point_on_cone(ctx, distribution_center, random_ggx_cos_theta(ctx, roughness));
}

vector3 random_isotropic_beckmann(random_ctx* ctx, vector3 distribution_center, scalar roughness)
{
  return random_point_on_cone(ctx, distribution_center, random_beckmann_cos_theta(ctx, roughness));
}

#endif
