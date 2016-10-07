#ifndef RANDOM_CL
#define RANDOM_CL

#include "math.cl"

typedef __global int random_ctx;

__constant const int random_max = 2147483647;
__constant const int random_a = 16807;

int random_serial_int(int* state)
{
  int s = *state;
  s = ((long)(s * random_a)) % random_max;
  *state = s;
  
  return s;
}

scalar random_serial_scalar(int* state)
{
  return (scalar)random_serial_int(state) / (scalar)random_max;
}

scalar random_uniform_scalar(random_ctx* ctx)
{
  int global_id = get_global_id(1) * get_global_size(0) + get_global_id(0);
  
  int state = ctx[global_id];
  scalar random_number = random_serial_scalar(&state);
  ctx[global_id] = state;
  
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
  int global_id = get_global_id(1) * get_global_size(0) + get_global_id(0);
  
  int state = ctx[global_id];
  int random_number = random_serial_int(&state);
  ctx[global_id] = random_number;
  
  return random_number;
}

vector3 random_uniform_sphere(random_ctx* ctx)
{
  scalar x1, x2;
  scalar r;
  do
  {
    x1 = 2.f * random_uniform_scalar(ctx) + 1.f;
    x2 = 2.f * random_uniform_scalar(ctx) + 1.f;
    r = x1 * x1;
    r += x2 * x2;
  } while(r >= 1.f);
  
  scalar sqrt_term = sqrt(1.f - r);
  
  vector3 result;
  result.x = 2 * x1 * sqrt_term;
  result.y = 2 * x2 * sqrt_term;
  result.z = 1.f - 2 * r;
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
        
#endif
