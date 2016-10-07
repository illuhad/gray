/*
 * To change ctx license header, choose License Headers in Project Properties.
 * To change ctx template file, choose Tools | Templates
 * and open the template in the editor.
 */


#ifndef OBJECTS_CL
#define OBJECTS_CL

#include "math.cl"

/************************ Types ************************************/
#include "common.cl_hpp"

typedef vector3 intensity;

typedef struct
{
  intensity scattered_fraction;
  intensity emitted_light;
  scalar transmittance;
  scalar refraction_index;
  scalar specular_power;
} material;

typedef struct
{
  //xyz = RGB
  __global float4* scattered_fraction;
  //xyz = RGB
  __global float4* emitted_light;
  //xyz = Transmittance, Refraction index, Specular power
  __global float4* transmittance_refraction_specular;

  int width;
  int height;
} material_map;

typedef struct
{
  __global float4* scattered_fraction;
  __global float4* emitted_light;
  __global float4* transmittance_refraction_specular;

  __global int* width;
  __global int* height;

  int num_material_maps;

  __global unsigned long* offsets;
} material_db;

typedef struct
{
  vector3 position;
  vector3 normal;
  float2  uv_coordinates;
  // interface between two materials
  object_entry hit_object_from;
  object_entry hit_object_to;
  material material_from;
  material material_to;
} path_vertex;

typedef struct
{
  intensity energy;
  vector3 direction;
  
  path_vertex origin_vertex;
} ray;

/********************** Geometries *********************************/

    /********************** Plane *********************/

__constant const scalar self_shadowing_epsilon = 1.e-3f;


int plane_geometry_intersects(const plane_geometry* ctx, const ray* r,
                              path_vertex* event)
{
  scalar direction_normal_projection = dot(ctx->normal, r->direction);
  
  if(direction_normal_projection == 0.f)
    return 0;
  
  vector3 v = ctx->position - r->origin_vertex.position;
  scalar s = dot(v, ctx->normal) / direction_normal_projection;
  
  if(s < 0.0f)
    // Intersection is behind ray origin
    return 0;
  
  event->position = r->origin_vertex.position;
  event->position += s * r->direction;
  event->normal = ctx->normal;

  // The standard plane of infinite size can
  // only be made of a uniform material.
  event->uv_coordinates = (float2)(0.f, 0.f);

  // impact normal should always point away from the object
  if(dot(ctx->normal, r->direction) > 0.0f)
    event->normal *= -1.f;
  
  return 1;
}

      /********************** Disk ***********************/


int disk_geometry_intersects(const disk_geometry* ctx, const ray* r,
                              path_vertex* event)
{
  if(plane_geometry_intersects(&(ctx->plane), r, event))
  {
    vector3 w = vec_from_to(ctx->plane.position, event->position);
    if(dot(w,w) <= ctx->radius * ctx->radius)
    {
      // TODO: Calculate uv coordinates

      return 1;
    }
  }
  
  return 0;
}

      /****************** Simple, thin lens *****************/


scalar simple_lens_object_get_aperture(const simple_lens_object* ctx)
{
  return ctx->geometry.radius;
}

void simple_lens_object_set_aperture(simple_lens_object* ctx, const scalar aperture)
{
  ctx->geometry.radius = aperture;
}

vector3 simple_lens_object_get_center(const simple_lens_object* ctx)
{
  return ctx->geometry.plane.position;
}

void simple_lens_object_set_center(simple_lens_object* ctx, vector3 center)
{
  ctx->geometry.plane.position = center;
}

void simple_lens_object_init(simple_lens_object* ctx,
                             const vector3 center,
                             const vector3 normal,
                             const scalar aperture)
{
  ctx->geometry.plane.normal = normal;
  simple_lens_object_set_center(ctx, center);
  simple_lens_object_set_aperture(ctx, aperture);
}

void simple_lens_object_propagate_ray(const simple_lens_object* ctx,
                               const path_vertex* impact,
                               random_ctx* rand,
                               ray* r)
{
  vector3 central_ray_direction = simple_lens_object_get_center(ctx) - r->origin_vertex.position;
  
  scalar central_ray_length = length(central_ray_direction);
  central_ray_direction *= 1.f / central_ray_length;
  
  scalar cos_normal_central_ray = dot(central_ray_direction, ctx->geometry.plane.normal);
  scalar object_distance = fabs(central_ray_length * 
                              cos_normal_central_ray);
  
  scalar image_distance = 1.f / (1.f/ctx->focal_length - 1.f/object_distance);
  scalar s = image_distance / cos_normal_central_ray;
  vector3 image_position = ctx->geometry.plane.position + fabs(s) * central_ray_direction;
  
  r->origin_vertex = *impact;
  r->direction = normalize(image_position - impact->position);
  r->origin_vertex.position += self_shadowing_epsilon * r->direction;
}

      /********************** Sphere ***********************/

int sphere_geometry_intersects(const sphere_geometry* ctx, const ray* r,
                              path_vertex* event)
{
  vector3 v = r->origin_vertex.position - ctx->position;
  scalar v_dot_d = dot(v, r->direction);
  scalar v2 = dot(v,v);
  
  scalar discriminant = v_dot_d * v_dot_d - v2 + ctx->radius * ctx->radius;
  if(discriminant < 0.f)
    return 0;
  
  scalar sqrt_discriminant = sqrt(discriminant);
  scalar s1 = -v_dot_d + sqrt_discriminant;
  scalar s2 = -v_dot_d - sqrt_discriminant;
  
  if(s1 < 0.f)
    // + solution is negative - we have no intersection ahead
    return 0;
  
  event->position = r->origin_vertex.position;
  
  if(s2 < 0.f)
    event->position += s1 * r->direction;
  else
    event->position += s2 * r->direction;
  
  event->normal = normalize(event->position - ctx->position);

  // Calculate uv coordinates for material mapping
  scalar mat_u = acospi(dot(ctx->polar_direction, event->normal)) * 0.5f + 0.5f;

  scalar x = dot(ctx->equatorial_basis1, event->normal);
  scalar y = dot(ctx->equatorial_basis2, event->normal);

  scalar mat_v = atan2pi(y, x) * 0.5f + 0.5f;
  event->uv_coordinates = (float2)(mat_u, mat_v);

  return 1;
}


#endif /* OBJECTS_CL */

