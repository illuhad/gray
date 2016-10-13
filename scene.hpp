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

#ifndef SCENE_HPP
#define SCENE_HPP

#include <vector>

#include "types.hpp"
#include "common.cl_hpp"
#include "qcl.hpp"
#include "material_map.hpp"

namespace gray {
namespace device_object {

class camera
{
public:
  camera(vector3 position, vector3 look_at, scalar roll_angle,
        scalar aperture, scalar focal_plane_distance)
  : _lens_plane_distance{1.0f},
    _roll_angle{roll_angle}
  {
    assert(aperture > 0.0);
    _camera_lens.geometry.plane.position = position;
    _camera_lens.geometry.radius = aperture;
    set_focal_plane_distance(focal_plane_distance);
    set_look_at(look_at);
  }

  void set_look_at(vector3 direction)
  {
    _look_at = direction;
    _camera_lens.geometry.plane.normal = direction;

    _position = _camera_lens.geometry.plane.position - _lens_plane_distance * _look_at;

    // Recalculate screen basis vectors
    vector3 v1 = VECTOR3(0, 0, 1);
    
    if (_look_at.s[0] == v1.s[0] &&
        _look_at.s[1] == v1.s[1] &&
        _look_at.s[2] == v1.s[2])
    {
      v1 = VECTOR3(1, 0, 0);
    }

    v1 = math::cross(v1, _look_at);
    vector3 v2 = math::cross(_look_at, v1);
    math::matrix3x3 roll_matrix;
    math::matrix_create_rotation_matrix(&roll_matrix, _look_at, _roll_angle);

    this->_screen_basis1 = matrix_vector_mult(&roll_matrix, v1);
    this->_screen_basis2 = matrix_vector_mult(&roll_matrix, v2);

    /*#ifndef WITH_LIGHT_RAYS
    matrix3x3 basis_matrix;
    basis_matrix.row0 = (vector3)(ctx->screen_basis1.x, ctx->screen_basis2.x, look_at.x);
    basis_matrix.row1 = (vector3)(ctx->screen_basis1.y, ctx->screen_basis2.y, look_at.y);
    basis_matrix.row2 = (vector3)(ctx->screen_basis1.z, ctx->screen_basis2.z, look_at.z);
  
    matrix_create_inverse(&(ctx->screen_space_transformation), &basis_matrix);
#endif*/
  }

  void set_position(vector3 pos)
  {
    _camera_lens.geometry.plane.position = pos;
    _position = _camera_lens.geometry.plane.position
              -_lens_plane_distance * _look_at;
  }

  void set_roll_angle(scalar roll_angle)
  {
    this->_roll_angle = roll_angle;
    // This recalculates screen basis vectors
    set_look_at(this->_look_at);
  }

  vector3 get_look_at() const
  {
    return _look_at;
  }

  vector3 get_position() const
  {
    return _camera_lens.geometry.plane.position;
  }

  vector3 get_screen_basis1() const
  {
    return _screen_basis1;
  }

  vector3 get_screen_basis2() const
  {
    return _screen_basis2;
  }

  scalar get_roll_angle() const
  {
    return _roll_angle;
  }

  scalar get_lens_plane_distance() const
  {
    return _lens_plane_distance;
  }

  scalar get_focal_length() const
  {
    if(_camera_lens.focal_length > 0.0)
      return _camera_lens.focal_length;
    return 1.0;
  }

  scalar get_aperture_diameter() const
  {
    return _camera_lens.geometry.radius;
  }

  void set_focal_plane_distance(scalar focal_plane_distance)
  {
    _camera_lens.focal_length = 1.f / (1.f / focal_plane_distance + 1.f / _lens_plane_distance);
  }

  void set_focal_length(scalar focal_length)
  {
    assert(focal_length > 0.0);
    _camera_lens.focal_length = focal_length;
  }

  scalar get_focal_plane_distance() const
  {
    return 1.f / (1.f / get_focal_length() - 1.f / _lens_plane_distance);
  }

  void enable_autofocus()
  {
    _camera_lens.focal_length = -1.0;
  }

  inline bool is_autofocus_enabled() const
  {
    return _camera_lens.focal_length <= 0.0;
  }

private:
  vector3 _look_at;
  vector3 _position;
  vector3 _screen_basis1;
  vector3 _screen_basis2;
  scalar _lens_plane_distance;
  simple_lens_object _camera_lens;
  scalar _roll_angle;
};


class scene
{
public:
  scene(const qcl::const_device_context_ptr& ctx,
        std::size_t background_texture_width,
        std::size_t background_texture_height,
        scalar far_clipping_distance = 1.e5f)
  : _ctx{ctx}, 
    _far_clipping_distance{far_clipping_distance},
    _materials{ctx}
  {
    // Allocate background map
    _materials.allocate_material_map(background_texture_width,
                                    background_texture_height);
  }

  scene(const scene& other) = delete;
  scene &operator=(const scene &other) = delete;

  void add_sphere(const vector3& position,
                  const vector3& polar_direction,
                  const vector3& equatorial_direction,
                  scalar radius,
                  portable_int material_id)
  {
    object_sphere_geometry geometry;
    geometry.geometry.position = position;
    geometry.geometry.radius = radius;
    geometry.geometry.equatorial_basis1 = equatorial_direction;
    geometry.geometry.equatorial_basis2 = math::cross(polar_direction, equatorial_direction);
    geometry.geometry.polar_direction = polar_direction;
    geometry.material_id = material_id;
    geometry.id = static_cast<portable_int>(_host_objects.size());

    object_entry entry;
    entry.id = static_cast<portable_int>(_host_objects.size());
    entry.local_id = static_cast<portable_int>(_host_spheres.size());
    entry.type = OBJECT_TYPE_SPHERE;

    _host_objects.push_back(entry);
    _host_spheres.push_back(geometry);
  }

  void add_plane(const vector3& position,
                  const vector3& normal,
                  portable_int material_id)
  {
    object_plane_geometry geometry;
    geometry.geometry.position = position;
    geometry.geometry.normal = normal;
    geometry.material_id = material_id;
    geometry.id = static_cast<portable_int>(_host_objects.size());

    object_entry entry;
    entry.id = static_cast<portable_int>(_host_objects.size());
    entry.local_id = static_cast<portable_int>(_host_planes.size());
    entry.type = OBJECT_TYPE_PLANE;

    _host_objects.push_back(entry);
    _host_planes.push_back(geometry);
  }

  void add_disk(const vector3& position,
                const vector3& normal,
                scalar radius,
                portable_int material_id)
  {
    object_disk_geometry geometry;
    geometry.geometry.radius = radius;
    geometry.geometry.plane.position = position;
    geometry.geometry.plane.normal = normal;
    geometry.material_id = material_id;
    geometry.id = static_cast<portable_int>(_host_objects.size());

    object_entry entry;
    entry.id = static_cast<portable_int>(_host_objects.size());
    entry.local_id = static_cast<portable_int>(_host_disks.size());
    entry.type = OBJECT_TYPE_DISK_PLANE;

    _host_objects.push_back(entry);
    _host_disks.push_back(geometry);
  }

  material_map access_background_map()
  {
    return _materials.get_material_map(0);
  }

  const material_db& get_materials() const
  {
    return _materials;
  }

  material_db& get_materials()
  {
    return _materials;
  }

  int get_num_spheres() const
  {
    return static_cast<int>(_host_spheres.size());
  }

  int get_num_planes() const
  {
    return static_cast<int>(_host_planes.size());
  }

  int get_num_disks() const
  {
    return static_cast<int>(_host_disks.size());
  }

  scalar get_far_clipping_distance() const
  {
    return _far_clipping_distance;
  }

  const cl::Buffer& get_objects() const
  {
    return _objects;
  }

  const cl::Buffer& get_spheres() const
  {
    return _spheres;
  }

  const cl::Buffer& get_planes() const
  {
    return _planes;
  }

  const cl::Buffer& get_disks() const
  {
    return _disks;
  }

  /// Performs a full data transfer to the device
  void transfer_data()
  {
    _materials.transfer_data();
    if (!_host_objects.empty())
    {
      _ctx->create_input_buffer<object_entry>(_objects,
                                              _host_objects.size(),
                                              _host_objects.data());

      if (!_host_spheres.empty())
        _ctx->create_input_buffer<object_sphere_geometry>(_spheres,
                                                          _host_spheres.size(),
                                                          _host_spheres.data());
      if(!_host_planes.empty())
        _ctx->create_input_buffer<object_plane_geometry>(_planes,
                                                         _host_planes.size(),
                                                         _host_planes.data());
      if(!_host_disks.empty())
        _ctx->create_input_buffer<object_disk_geometry>(_disks,
                                                        _host_disks.size(),
                                                        _host_disks.data());
    }
  }

private:
  qcl::const_device_context_ptr _ctx;

  std::vector<object_entry> _host_objects;
  std::vector<object_sphere_geometry> _host_spheres;
  std::vector<object_plane_geometry> _host_planes;
  std::vector<object_disk_geometry> _host_disks;

  scalar _far_clipping_distance;

  cl::Buffer _objects;
  cl::Buffer _spheres;
  cl::Buffer _planes;
  cl::Buffer _disks;

  material_db _materials;
};
}
}

#endif
