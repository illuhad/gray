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

#ifndef MATERIAL_CL
#define MATERIAL_CL

#include "random.cl"
#include "objects.cl"

float4 array2d_extract_interpolated(texture data,
                                    float2 coord)
{
  int width = data.width;
  int height = data.height;


  int max_x = width - 1;
  int max_y = height - 1;

  float2 abs_coord = (float2)(coord.x * (float)max_x,
                              coord.y * (float)max_y);

  int x = (int)abs_coord.x;
  int y = (int)abs_coord.y;

  x = clamp(x, 0, max_x);
  y = clamp(y, 0, max_y);
  int xp1 = clamp(x + 1, 0, max_x);
  int yp1 = clamp(y + 1, 0, max_y);

  float4 x0y0 = data.data[x   * height + y  ];
  float4 x0y1 = data.data[x   * height + yp1];
  float4 x1y0 = data.data[xp1 * height + y  ];
  float4 x1y1 = data.data[xp1 * height + yp1];

  // Bilinear Interpolation
  float f_x = abs_coord.x - (float)x;
  float f_y = abs_coord.y - (float)y;
  float f_x_inv = 1.f - f_x;
  float f_y_inv = 1.f - f_y;

  return f_x_inv * f_y_inv * x0y0 
       + f_x     * f_y_inv * x1y0 
       + f_x_inv * f_y     * x0y1 
       + f_x     * f_y     * x1y1;
}

material material_map_get_material(const material_map* ctx,
                                   float2 coord)
{
  float2 clamped_coord = (float2)(clamp(coord.x, 0.0f, 1.0f), 
                                  clamp(coord.y, 0.0f, 1.0f));
  material mat;
  float4 scattered_fraction_data =
      array2d_extract_interpolated(ctx->scattered_fraction,
                                   clamped_coord);
  float4 emitted_light_data =
      array2d_extract_interpolated(ctx->emitted_light,
                                   clamped_coord);
  float4 transmittance_refraction_spec_data =
      array2d_extract_interpolated(ctx->transmittance_refraction_roughness,
                                   clamped_coord);

  mat.scattered_fraction = scattered_fraction_data.xyz;
  mat.emitted_light = emitted_light_data.xyz;
  mat.transmittance = transmittance_refraction_spec_data.x;
  mat.refraction_index = transmittance_refraction_spec_data.y;
  mat.roughness = transmittance_refraction_spec_data.z;

  return mat;
}

texture material_db_get_texture(const material_db* ctx, texture_id tex)
{
  texture result;

  unsigned long long offset = ctx->offsets[tex];
  result.data = ctx->data_buffer + offset;
  result.width =  ctx->width[tex];
  result.height = ctx->height[tex];

  return result;
}

material_map material_db_get_material_map(const material_db* ctx, int map_id)
{
  material_map mmap;

  material_db_entry entry = ctx->materials[map_id];

  mmap.scattered_fraction = material_db_get_texture(ctx, entry.scattered_fraction_texture_id);
  mmap.emitted_light = material_db_get_texture(ctx, entry.emitted_ligt_texture_id);
  mmap.transmittance_refraction_roughness =
      material_db_get_texture(ctx, entry.transmittance_refraction_roughness_texture_id);

  return mmap;
}

material material_db_get_material(const material_db* ctx, int map_id, float2 coord)
{
  material_map mmap = material_db_get_material_map(ctx, map_id);
  return material_map_get_material(&mmap, coord);
}

/// Calculates refracted vector
/// \return 1 if refraction can occur, 0 if there is total refraction
int material_refract(const material* ctx,
                     vector3 normal,
                     vector3 incident,
                     scalar n1,
                     scalar n2,
                     vector3* refracted)
{
  scalar n_ratio = n1 / n2;
  scalar cos_theta = -dot(incident, normal);
  scalar sin_theta_out2 = n_ratio * n_ratio * (1.f - cos_theta * cos_theta);
  if(sin_theta_out2 > 1.f)
    // Total reflection
    return 0;
  
  *refracted = n_ratio * incident;
  
  scalar lambda = n_ratio * cos_theta - sqrt(1.f - sin_theta_out2);
  *refracted += lambda * normal;
  
  return 1;
}

void material_remap_to_hemisphere(const material* ctx,
                                  const vector3 normal,
                                  vector3* v)
{
  scalar v_dot_n = dot(*v, normal);
  if(v_dot_n < 0.f)
    *v += 2.f * v_dot_n * normal;
}

scalar material_fresnel_reflectance(const material* ctx,
                                    scalar cos_theta_incident,
                                    scalar cos_theta_transmitted,
                                    scalar n1,
                                    scalar n2,
                                    int total_reflection)
{
  scalar reflectance = 1.0f;
  
  if(!total_reflection)
  {
    scalar r_perpendicular =(n1 * cos_theta_incident - n2 * cos_theta_transmitted) /
                            (n1 * cos_theta_incident + n2 * cos_theta_transmitted);
    scalar r_parallel = (n2 * cos_theta_incident - n1 * cos_theta_transmitted) /
                        (n2 * cos_theta_incident + n1 * cos_theta_transmitted);
      
    r_perpendicular *= r_perpendicular;
    r_parallel *= r_parallel;
      
    reflectance = 0.5 * (r_perpendicular + r_parallel);
  }
  
  return reflectance;
}

vector3 material_reflect(const material* ctx,
                         vector3 anti_incident_normal, vector3 incident_direction)
{
  vector3 outgoing = incident_direction;
  outgoing += -2 * dot(anti_incident_normal, incident_direction) * anti_incident_normal;
  return outgoing;
}

void material_propagate_ray(const material* ctx,
                            const path_vertex* impact,
                            random_ctx* rand,
                            ray* r)
{
  
  scalar refraction_idx_n1 = impact->material_from.refraction_index;
  scalar refraction_idx_n2 = impact->material_to.refraction_index;
  
  // Sample normal direction
  vector3 normal = random_isotropic_beckmann(rand, impact->normal, ctx->roughness);
  //vector3 normal = random_power_cosine(rand, impact->normal, ctx->roughness);
  //if(get_global_id(0) == 512 && get_global_id(1) == 512)
  //  printf("%f %f %f\n", normal.x, normal.y, normal.z);
  if (dot(normal, r->direction) > 0.f)
    normal *= -1.f;

  scalar random_sample = random_uniform_scalar(rand);
  if (random_sample > ctx->transmittance)
  {
    // Reflect ray
    vector3 reflected_direction = material_reflect(ctx,
                                                 normal, 
                                                 r->direction);

    r->direction = reflected_direction;
  }
  else
  {
    // Transmit ray
    vector3 refracted_direction;
    int total_reflection = !material_refract(ctx,
                                             normal,
                                             r->direction,
                                             refraction_idx_n1, refraction_idx_n2,
                                             &refracted_direction);

    scalar cos_theta_incident = fabs(dot(impact->normal, r->direction));
    scalar cos_theta_transmitted = fabs(dot(impact->normal, refracted_direction));

    scalar fresnel_reflectance = material_fresnel_reflectance(ctx,
                                                              cos_theta_incident,
                                                              cos_theta_transmitted,
                                                              refraction_idx_n1,
                                                              refraction_idx_n2,
                                                              total_reflection);

    random_sample = random_uniform_scalar(rand);
    scalar fresnel_transmittance = 1.f - fresnel_reflectance;

    if (random_sample > fresnel_transmittance)
    {
      vector3 reflected_direction = material_reflect(ctx,
                                                 normal, 
                                                 r->direction);
      r->direction = reflected_direction;
    }
    else
    {
      r->direction = refracted_direction;
    }
  }

  r->origin_vertex = *impact;
  r->origin_vertex.position += self_shadowing_epsilon * r->direction;
}

#endif /* MATERIAL_CL */

