#ifndef SCENE_HPP
#define SCENE_HPP

#include <vector>

#include "types.hpp"
#include "common.cl_hpp"
#include "qcl.hpp"

namespace gray {
namespace device_object {

class camera
{
public:
  camera(vector3 position, vector3 look_at, scalar roll_angle,
        scalar aperture, scalar focal_length)
  : _lens_plane_distance{1.0f},
    _roll_angle{roll_angle}
  {
    _camera_lens.geometry.plane.position = position;
    _camera_lens.geometry.radius = aperture;
    _camera_lens.focal_length = focal_length;
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

    v1 -= math::dot(v1, _look_at) * _look_at;
    v1 = math::normalize(v1);

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
    return _position;
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
    return _camera_lens.focal_length;
  }

  scalar get_aperture_diameter() const
  {
    return _camera_lens.geometry.radius;
  }

  void set_focal_length(scalar focal_length)
  {
    _camera_lens.focal_length = focal_length;
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

private:
  std::vector<object_entry> _host_objects;
  std::vector<object_sphere_geometry> _host_spheres;
  std::vector<object_plane_geometry> _host_planes;
  std::vector<object_disk_geometry> _host_disks;

  scalar _far_clipping_distance;

  cl::Buffer _objects;
  cl::Buffer _spheres;
  cl::Buffer _planes;
  cl::Buffer _disks;
};

}
}

#endif
