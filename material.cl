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

float4 array2d_extract_interpolated(__global const float4* ctx, 
                                    int width, int height, 
                                    float2 coord)
{
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

  float4 x0y0 = ctx[x   * height + y  ];
  float4 x0y1 = ctx[x   * height + yp1];
  float4 x1y0 = ctx[xp1 * height + y  ];
  float4 x1y1 = ctx[xp1 * height + yp1];

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

material material_map_get_material(const material_map *ctx,
                                   float2 coord)
{
  float2 clamped_coord = (float2)(clamp(coord.x, 0.0f, 1.0f), 
                                  clamp(coord.y, 0.0f, 1.0f));
  material mat;
  float4 scattered_fraction_data =
      array2d_extract_interpolated(ctx->scattered_fraction,
                                   ctx->width, ctx->height,
                                   clamped_coord);
  float4 emitted_light_data =
      array2d_extract_interpolated(ctx->emitted_light,
                                   ctx->width, ctx->height,
                                   clamped_coord);
  float4 transmittance_refraction_spec_data =
      array2d_extract_interpolated(ctx->transmittance_refraction_specular,
                                   ctx->width, ctx->height,
                                   clamped_coord);

  mat.scattered_fraction = scattered_fraction_data.xyz;
  mat.emitted_light = emitted_light_data.xyz;
  mat.transmittance = transmittance_refraction_spec_data.x;
  mat.refraction_index = transmittance_refraction_spec_data.y;
  mat.specular_power = transmittance_refraction_spec_data.z;

  return mat;
}


material_map material_db_get_material_map(const material_db* ctx, int map_id)
{
  unsigned long long offset = ctx->offsets[map_id];
  material_map mmap;
  mmap.scattered_fraction = ctx->scattered_fraction + offset;
  mmap.emitted_light = ctx->emitted_light + offset;
  mmap.transmittance_refraction_specular = ctx->transmittance_refraction_specular + offset;

  mmap.width  = ctx->width[map_id];
  mmap.height = ctx->height[map_id];

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
  
  vector3 anti_incident_normal = impact->normal;
  if(dot(r->direction, anti_incident_normal) > 0.f)
    anti_incident_normal *= -1.f;
  
  vector3 refracted_direction;
  int total_reflection = !material_refract(ctx,
                                          anti_incident_normal, 
                                          r->direction, 
                                          refraction_idx_n1, refraction_idx_n2,
                                          &refracted_direction);
  
  
  scalar cos_theta_incident = fabs(dot(impact->normal, r->direction));
  scalar cos_theta_transmitted = fabs(dot(impact->normal, refracted_direction));
  
  scalar reflectance = material_fresnel_reflectance(ctx,
                                                    cos_theta_incident,
                                                    cos_theta_transmitted,
                                                    refraction_idx_n1,
                                                    refraction_idx_n2,
                                                    total_reflection);
  
  vector3 reflected_direction = material_reflect(ctx,
                                                 anti_incident_normal, 
                                                 r->direction);
  
  vector3 outgoing_normal = anti_incident_normal;
  vector3 cosine_center;
  if(random_uniform_scalar(rand) > (1.f - reflectance) * ctx->transmittance)
  {
    // Reflect ray
    cosine_center = reflected_direction;
  }
  else
  {
    // Transmit ray
    if(!total_reflection)
    {
      cosine_center = refracted_direction;
      outgoing_normal *= -1.f;
    }
    else
    {
      cosine_center = reflected_direction;
    }
  }
  
  vector3 new_direction;
  scalar cos_theta;
  do
  {
    new_direction = random_power_cosine(rand, cosine_center, ctx->specular_power);
    cos_theta = fabs(dot(impact->normal, new_direction));
  } while(random_uniform_scalar(rand) > cos_theta);
  material_remap_to_hemisphere(ctx, outgoing_normal, &new_direction);

  r->direction = new_direction;
  r->origin_vertex = *impact;
  r->origin_vertex.position += self_shadowing_epsilon * outgoing_normal;
}

#endif /* MATERIAL_CL */

