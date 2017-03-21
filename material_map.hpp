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


#ifndef MATERIAL_MAP_HPP
#define MATERIAL_MAP_HPP

#include <cassert>
#include "qcl.hpp"
#include "types.hpp"
#include "common.cl_hpp"

namespace gray {

namespace device_object {
class material_db;
}

class texture_accessor
{
public:
  texture_accessor(float4* buffer, cl_int width, cl_int height)
    : _data{buffer},
      _width{static_cast<std::size_t>(width)},
      _height{static_cast<std::size_t>(height)}
  {}

  texture_accessor(const texture_accessor &other) = default;
  texture_accessor &operator=(const texture_accessor &other) = default;

  const float4& read(std::size_t x, std::size_t y) const
  {
    return _data[x * this->_height + y];
  }

  void write(const float4& c, std::size_t x, std::size_t y)
  {
    assert(x < _width);
    assert(y < _height);
    _data[x * this->_height + y] = c;
  }

  inline
  std::size_t get_width() const
  {
    return _width;
  }

  inline
  std::size_t get_height() const
  {
    return _height;
  }

  inline void fill(float4 fill_color)
  {
    for(std::size_t i = 0; i < _width; ++i)
      for(std::size_t j = 0; j < _height; ++j)
        write(fill_color, i, j);
  }

private:
  device_object::material_db *_mat_db;

  float4* _data;
  std::size_t _width;
  std::size_t _height;
};

class material_map
{
public:
  material_map(const texture_accessor& scattered_fraction,
              const texture_accessor& emitted_light,
              const texture_accessor& transmittance_refraction_roughness)
    : _scattered_fraction{scattered_fraction},
      _emitted_light{emitted_light},
      _transmittance_refraction_roughness{transmittance_refraction_roughness}
    {}
  
  texture_accessor get_scattered_fraction()
  {
    return _scattered_fraction;
  }

  const texture_accessor get_scattered_fraction() const
  {
    return _scattered_fraction;
  }

  texture_accessor get_emitted_light()
  {
    return _emitted_light;
  }

  const texture_accessor get_emitted_light() const
  {
    return _emitted_light;
  }

  texture_accessor get_transmittance_refraction_roughness()
  {
    return _transmittance_refraction_roughness;
  }
  const texture_accessor get_transmittance_refraction_roughness() const
  {
    return _transmittance_refraction_roughness;
  }

private:
  texture_accessor _scattered_fraction;
  texture_accessor _emitted_light;
  texture_accessor _transmittance_refraction_roughness;
};

namespace device_object {

class material_db
{
public:

  material_db(const qcl::const_device_context_ptr& ctx)
  : _ctx{ctx}
  {}

  /// Allocate a new material map - prepares a fresh material_db if \c purge_host_memory()
  /// has been called. 
  texture_id allocate_texture(std::size_t width, std::size_t height)
  {
    std::size_t num_texels = width * height;
    _host_data_buffer.resize(_host_data_buffer.size() + num_texels);

    _host_widths.push_back(static_cast<cl_int>(width));
    _host_heights.push_back(static_cast<cl_int>(height));
    if(!_host_offsets.empty())
      _host_offsets.push_back(_host_offsets.back() + num_texels);
    else
    {
      _host_offsets.push_back(0);
      _host_offsets.push_back(num_texels);
    }

    _num_textures = static_cast<int>(_host_offsets.size() - 1);

    return _num_textures - 1;
  }

  material_map get_material_map(material_id index)
  {
    assert(static_cast<std::size_t>(index) < _host_offsets.size());

    material_db_entry db_entry = this->_host_materials[index];
    cl_ulong offset = static_cast<cl_ulong>(_host_offsets[index]);

    return material_map(access_texture(db_entry.scattered_fraction_texture_id),
                        access_texture(db_entry.emitted_ligt_texture_id),
                        access_texture(db_entry.transmittance_refraction_roughness_texture_id));
  }

  material_id create_material(texture_id scattered_fraction_texture,
                              texture_id emitted_light_texture,
                              texture_id transmittance_refraction_roughness_texture)
  {
    material_db_entry material;

    material.scattered_fraction_texture_id = scattered_fraction_texture;
    material.emitted_ligt_texture_id = emitted_light_texture;
    material.transmittance_refraction_roughness_texture_id = transmittance_refraction_roughness_texture;

    _host_materials.push_back(material);

    return static_cast<material_id>(_host_materials.size() - 1);
  }

  cl_int get_num_textures() const
  {
    return static_cast<cl_int>(_num_textures);
  }

  cl_int get_num_materials() const
  {
    return static_cast<cl_int>(_host_materials.size());
  }

  const cl::Buffer& get_texture_data_buffer() const
  {
    return _data_buffer;
  }
  
  const cl::Buffer& get_materials() const
  {
    return _materials;
  }

  const cl::Buffer& get_widths() const
  {
    return _width;
  }

  const cl::Buffer& get_heights() const
  {
    return _height;
  }

  const cl::Buffer& get_offsets() const
  {
    return _offsets;
  }

  void transfer_data()
  {
    if (this->_num_textures == 0)
      return;

    // Resize buffers and perform full data transfer
    
    _ctx->create_input_buffer<float4>(_data_buffer,
                                      _host_data_buffer.size(),
                                      _host_data_buffer.data());
    _ctx->create_input_buffer<material_db_entry>(_materials,
                                                 _host_materials.size(),
                                                 _host_materials.data());
    _ctx->create_input_buffer<cl_int>(_width,
                                      _host_widths.size(),
                                      _host_widths.data());
    _ctx->create_input_buffer<cl_int>(_height,
                                      _host_heights.size(),
                                      _host_heights.data());
    _ctx->create_input_buffer<cl_ulong>(_offsets,
                                        _host_offsets.size(),
                                        _host_offsets.data());
  }

  /// Purge host memory. Materials already committed to
  /// the device cannot be modified afterwards using modifications
  /// of host memory.
  void purge_host_memory()
  {
    _host_data_buffer.clear();
    _host_heights.clear();
    _host_widths.clear();
    _host_offsets.clear();
    _host_materials.clear();
  }


  texture_accessor access_texture(texture_id tex)
  {
    cl_ulong offset = _host_offsets[static_cast<std::size_t>(tex)];
    cl_int width = _host_widths[static_cast<std::size_t>(tex)];
    cl_int height = _host_heights[static_cast<std::size_t>(tex)];
    return texture_accessor{_host_data_buffer.data() + offset, width, height};
  }

private:

  std::vector<float4> _host_data_buffer;
  std::vector<material_db_entry> _host_materials;
  std::vector<cl_int> _host_widths;
  std::vector<cl_int> _host_heights;
  std::vector<cl_ulong> _host_offsets;

  cl::Buffer _data_buffer;
  cl::Buffer _materials;
  cl::Buffer _width;
  cl::Buffer _height;
  int _num_textures;
  cl::Buffer _offsets;

  qcl::const_device_context_ptr _ctx;
};

} // device_object

} // gray

#endif
