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

#ifndef SCENE_CL
#define SCENE_CL

#include "math.cl"
#include "objects.cl"
#include "material.cl"


#define DEFINE_OBJECT_TYPE_BUFFER(geometry_type, name) \
  __global OBJECT_NAME(geometry_type)* name; \
  int num_ ## name

typedef struct
{
  __global object_entry* objects;
  
  DEFINE_OBJECT_TYPE_BUFFER(sphere_geometry, spheres);
  DEFINE_OBJECT_TYPE_BUFFER(plane_geometry, planes);
  DEFINE_OBJECT_TYPE_BUFFER(disk_geometry, disks);
  
  int num_objects;
  scalar far_clipping_distance;

  material_db materials;
  object_entry background_object;

} scene;

void scene_init(scene* ctx,
                __global object_entry* objects, int num_objects,
                __global OBJECT_NAME(sphere_geometry)* spheres, int num_spheres,
                __global OBJECT_NAME(plane_geometry)* planes, int num_planes,
                __global OBJECT_NAME(disk_geometry)* disks, int num_disks,
                scalar far_clipping_distance)
{
  ctx->objects = objects;
  ctx->num_objects = num_objects;

  ctx->num_spheres = num_spheres;
  ctx->spheres = spheres;

  ctx->num_planes = num_planes;
  ctx->planes = planes;

  ctx->num_disks = num_disks;
  ctx->disks = disks;

  ctx->background_object.type = OBJECT_TYPE_BACKGROUND;
  ctx->background_object.id = BACKGROUND_ID;
  ctx->background_object.local_id = 0;

  ctx->far_clipping_distance = far_clipping_distance;
}

#define OBJECTS_INTERSECTS(geometry_type, object_ptr, ray_ptr, path_vertex_ptr) \
  (geometry_type ## _intersects(object_ptr, ray_ptr, path_vertex_ptr))


object_type scene_get_object_type(scene* ctx, object_id id)
{
  return ctx->objects[id].type;
}

int scene_get_num_objects(const scene* ctx)
{
  return ctx->num_objects;
}

void scene_get_medium(const scene* ctx, vector3 position, 
                      object_entry* medium_entry, 
                      material* medium_material)
{
  // For now, constant medium everywhere
  medium_entry->id = MEDIUM_ID;
  medium_entry->local_id = 0;
  medium_entry->type = OBJECT_TYPE_SURROUNDING_MEDIUM;

  medium_material->scattered_fraction = (intensity)(1, 1, 1);
  medium_material->emitted_light = (intensity)(0, 0, 0);
  medium_material->refraction_index = 1.f;
  medium_material->specular_power = 0.0f;
  medium_material->transmittance = 1.0f;


}

#define OBJECT_LIST_GET_NEAREST_INTERSECTION(geometry_type,                     \
                                            num_objects,                        \
                                             object_list,                       \
                                             object_entries,                    \
                                             material_database,                 \
                                             min_dist2,                         \
                                             nearest_intersection_vertex,       \
                                             nearest_intersection_obj,          \
                                             hit_material,                      \
                                             r,                                 \
                                            vertex)                             \
  for (int i = 0; i < (num_objects); ++i)                                       \
  {                                                                             \
    OBJECT_NAME(geometry_type)  current = (object_list)[i];                     \
    if (OBJECTS_INTERSECTS(geometry_type, &(current.geometry), r, vertex))      \
    {                                                                           \
      vector3 delta = vec_from_to(r->origin_vertex.position, vertex->position); \
      scalar dist2 = dot(delta, delta);                                         \
      if (dist2 < min_dist2)                                                    \
      {                                                                         \
        min_dist2 = dist2;                                                      \
        nearest_intersection_vertex = *vertex;                                  \
        hit_material = material_db_get_material(&material_database,             \
                                                current.material_id,            \
                                                vertex->uv_coordinates);        \
        nearest_intersection_obj = object_entries[current.id];                  \
      }                                                                         \
    }                                                                           \
  }

void scene_get_nearest_intersection(const scene* ctx, const ray* r, path_vertex* vertex)
{
  object_entry nearest_intersection_obj;
  path_vertex nearest_intersection_vertex;
  scalar min_dist2 = ctx->far_clipping_distance;
  material hit_material;

  OBJECT_LIST_GET_NEAREST_INTERSECTION(sphere_geometry,
                                       ctx->num_spheres,
                                       ctx->spheres,
                                       ctx->objects,
                                       ctx->materials,
                                       min_dist2,
                                       nearest_intersection_vertex,
                                       nearest_intersection_obj,
                                       hit_material,
                                       r, vertex);

  OBJECT_LIST_GET_NEAREST_INTERSECTION(plane_geometry,
                                       ctx->num_planes,
                                       ctx->planes,
                                       ctx->objects,
                                       ctx->materials,
                                       min_dist2,
                                       nearest_intersection_vertex,
                                       nearest_intersection_obj,
                                       hit_material,
                                       r, vertex);

  OBJECT_LIST_GET_NEAREST_INTERSECTION(disk_geometry,
                                       ctx->num_disks,
                                       ctx->disks,
                                       ctx->objects,
                                       ctx->materials,
                                       min_dist2,
                                       nearest_intersection_vertex,
                                       nearest_intersection_obj,
                                       hit_material,
                                       r, vertex);


  object_entry medium;
  material medium_material;

  if (min_dist2 == ctx->far_clipping_distance)
  {
    vector3 impact = r->origin_vertex.position
                   + ctx->far_clipping_distance * r->direction;
    scene_get_medium(ctx, impact, &medium, &medium_material);

    // We have not found any objects closer than the far clipping distance - return background
    float2 uv_background_coordinates;

    // cos_theta = dot(r->direction, [0,0,1])
    scalar v = acospi(r->direction.z);
    scalar u = atan2pi(r->direction.y, r->direction.x) * 0.5f + 0.5f;
    uv_background_coordinates.x = u;
    uv_background_coordinates.y = v;

    hit_material = material_db_get_material(&(ctx->materials),
                                            BACKGROUND_MATERIAL,
                                            uv_background_coordinates);   

    vertex->material_to = hit_material;
    vertex->material_from = medium_material;
    vertex->normal = -(r->direction);
    vertex->position = impact;
    vertex->hit_object_from = medium;
    vertex->hit_object_to = ctx->background_object;
  }
  else
  {
    scene_get_medium(ctx, 
                    nearest_intersection_vertex.position, 
                    &medium, 
                    &medium_material);

    if (dot(nearest_intersection_vertex.normal, r->direction) < 0.f)
    {
      // Incoming ray
      nearest_intersection_vertex.hit_object_from = medium;
      nearest_intersection_vertex.hit_object_to = nearest_intersection_obj;
      nearest_intersection_vertex.material_from = medium_material;
      nearest_intersection_vertex.material_to = hit_material;
    }
    else
    {
      // Outgoing ray
      nearest_intersection_vertex.hit_object_from = nearest_intersection_obj;
      nearest_intersection_vertex.hit_object_to = medium;
      nearest_intersection_vertex.material_from = hit_material;
      nearest_intersection_vertex.material_to = medium_material;
    }

    *vertex = nearest_intersection_vertex;
  }
}



#endif
