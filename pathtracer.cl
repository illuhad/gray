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

#ifndef PATHTRACER_CL
#define PATHTRACER_CL

#include "random.cl"
#include "scene.cl"


typedef uchar3 rgb_color; 
typedef float4 rgba_color;


#define MAX_BOUNCES 256

typedef struct
{
  vector3 look_at;
  vector3 position;
  
  vector3 screen_basis1;
  vector3 screen_basis2;
  
  scalar lens_plane_distance;
  simple_lens_object camera_lens;

  scalar roll_angle;
} camera;


void camera_generate_ray(const camera* ctx, random_ctx* rand,
                        int width, int height,
                        int px_x, int px_y, ray* r)
{
  // First generate a sample for the position in the pixel
  scalar px_size = 1.0f / (scalar)width;
  
  scalar position_in_pixel_x = random_uniform_scalar(rand);
  scalar position_in_pixel_y = random_uniform_scalar(rand);
  
  scalar px_relative_to_center_x = (scalar)(px_x - width / 2);
  scalar px_relative_to_center_y = (scalar)(px_y - height/ 2);
  
  vector3 origin = ctx->position;
  origin += (px_relative_to_center_x - 0.5f + position_in_pixel_x) * px_size * ctx->screen_basis1;
  origin += (px_relative_to_center_y - 0.5f + position_in_pixel_y) * px_size * ctx->screen_basis2;

  // Generate a sample for the direction
  scalar x,y;
  scalar r2 = ctx->camera_lens.geometry.radius * ctx->camera_lens.geometry.radius;

  do
  {
    x = random_uniform_scalar_minmax(rand, 
                                     -ctx->camera_lens.geometry.radius,
                                     ctx->camera_lens.geometry.radius);
    y = random_uniform_scalar_minmax(rand, 
                                     -ctx->camera_lens.geometry.radius,
                                     ctx->camera_lens.geometry.radius);
  } while (x * x + y * y > r2);

  vector3 lens_sample = simple_lens_object_get_center(&(ctx->camera_lens));
  lens_sample += x * ctx->screen_basis1;
  lens_sample += y * ctx->screen_basis2;
  
  vector3 ray_direction = normalize(lens_sample - origin);
  
  r->direction = ray_direction;
  r->origin_vertex.position = origin;
  r->origin_vertex.normal = -1.f * ctx->look_at;
  
  r->energy = (intensity)(1, 1, 1);

  path_vertex impact;
  disk_geometry_intersects(&(ctx->camera_lens.geometry), r, &impact);
  simple_lens_object_propagate_ray(&(ctx->camera_lens), &impact, rand, r);
}

void camera_autofocus(camera* ctx, const scene* s)
{
  ray test_ray;
  test_ray.origin_vertex.position = ctx->camera_lens.geometry.plane.position;
  test_ray.direction = ctx->look_at;
  path_vertex intersection;
  scene_get_nearest_intersection(s, &test_ray, &intersection);
  scalar dist = distance(intersection.position, test_ray.origin_vertex.position);

  ctx->camera_lens.focal_length = 1.f / (1.f / dist + 1.f / ctx->lens_plane_distance);
}

intensity evaluate_ray(ray* r, random_ctx* rand, const scene* s)
{
  path_vertex next_intersection;
  intensity radiance = (intensity)(0, 0, 0);
  for (int i = 0; i < MAX_BOUNCES; ++i) // we will never really iterate to the end
  {
    scene_get_nearest_intersection(s, r, &next_intersection);

    material *interacting_material;
    if (dot(next_intersection.normal, r->direction) < 0.f)
      // Incoming ray
      interacting_material = &(next_intersection.material_to);
    else
      // Outgoing ray
      interacting_material = &(next_intersection.material_from);

    // Russian roulette
    scalar russian_roulette_probability =
        fmax(r->energy.x,
            fmax(r->energy.y, 
                r->energy.z));
    
    intensity effective_bsdf = 
      interacting_material->scattered_fraction * (1.f / russian_roulette_probability);

    radiance += fabs(dot(next_intersection.normal, r->direction)) 
              * r->energy * interacting_material->emitted_light;
    r->energy *= effective_bsdf;

    if(random_uniform_scalar(rand) < russian_roulette_probability)
      material_propagate_ray(interacting_material,
                             &next_intersection,
                             rand,
                             r);
    else
      return radiance;
  }
  return radiance;
}

__constant sampler_t pixel_sampler = CLK_NORMALIZED_COORDS_FALSE | 
                                     CLK_ADDRESS_CLAMP_TO_EDGE |
                                     CLK_FILTER_NEAREST;

__kernel void trace_paths(__write_only image2d_t pixels,
                          __read_only image2d_t current_render_state, //output of previous kernel
                          int num_previous_rays,
                          __global int *random_state,
                          camera cam,
                          int rays_per_pixel,

                          // Scene definition
                          __global object_entry *objects,
                          __global object_sphere_geometry *spheres,
                          __global object_plane_geometry *planes,
                          __global object_disk_geometry *disks,
                          int num_spheres,
                          int num_planes,
                          int num_disks,
                          float far_clipping_distance,
                          
                          // Material Database
                          __global float4 * scattered_fraction_maps,
                          __global float4 *emitted_light_maps,
                          __global float4 *transmittance_refraction_specular_maps,
                          __global int *widths,
                          __global int *heights,
                          __global unsigned long *offsets,
                          int num_material_maps)
{
  // Get resolution of render window
  int width = get_image_width(pixels);
  int height = get_image_height(pixels);

  // Determine which pixel this thread will process
  int px_x = get_global_id(0);
  int px_y = get_global_id(1);

  if(px_x < width && px_y < height)
  {
    // Initialize scene object
    int num_objects = num_spheres + num_planes + num_disks;
    scene s;
    scene_init(&s, objects, num_objects,
               spheres, num_spheres,
               planes, num_planes,
               disks, num_disks,
               far_clipping_distance);

    s.materials.scattered_fraction = scattered_fraction_maps;
    s.materials.emitted_light = emitted_light_maps;
    s.materials.transmittance_refraction_specular =
        transmittance_refraction_specular_maps;
    s.materials.width = widths;
    s.materials.height = heights;
    s.materials.offsets = offsets;
    s.materials.num_material_maps = num_material_maps;

    // Calculate focal distance if autofocus is enabled
    if(cam.camera_lens.focal_length <= 0.0f)
      camera_autofocus(&cam, &s);

    // Actual work starts here
    intensity pixel_value = (intensity)(0, 0, 0);
    ray r;
    for (int i = 0; i < rays_per_pixel; ++i)
    {
      camera_generate_ray(&cam, random_state, width, height, px_x, px_y, &r);

      pixel_value += evaluate_ray(&r, random_state, &s);
    }

    // Save result
    int2 coord = (int2)(px_x, px_y);

    rgba_color previous_result = read_imagef(current_render_state, pixel_sampler, coord);
    scalar total_ray_number = (scalar)rays_per_pixel + (scalar)num_previous_rays;

    rgba_color color;
    color.xyz = pixel_value / total_ray_number;
    color += previous_result * (scalar)num_previous_rays / total_ray_number;
    color.w = 1.f;

    write_imagef(pixels, coord, color);
  }
}



#endif
