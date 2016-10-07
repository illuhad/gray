
#ifndef MATERIAL_MAP_HPP
#define MATERIAL_MAP_HPP

#include <cassert>
#include "qcl.hpp"
#include "types.hpp"


namespace gray {

namespace device_object {
class material_db;
}

class texture
{
public:
  texture(float4* buffer, std::size_t width, std::size_t height)
  : _data{buffer}, _width{width}, _height{height}
  {}

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

private:
  device_object::material_db *_mat_db;

  float4* _data;
  std::size_t _width;
  std::size_t _height;
};

class material_map
{
public:
  material_map(float4* scattered_fraction,
              float4* emitted_light,
              float4* transmittance_refraction_specular,
              std::size_t width, std::size_t height)
  : _scattered_fraction{scattered_fraction, width, height},
    _emitted_light{emitted_light, width, height},
    _transmittance_refraction_specular{transmittance_refraction_specular, width, height}
    {}
  
  texture& get_scattered_fraction()
  {
    return _scattered_fraction;
  }

  const texture& get_scattered_fraction() const
  {
    return _scattered_fraction;
  }

  texture& get_emitted_light()
  {
    return _emitted_light;
  }

  const texture& get_emitted_light() const
  {
    return _emitted_light;
  }

  texture& get_transmittance_refraction_specular()
  {
    return _transmittance_refraction_specular;
  }
  const texture& get_transmittance_refraction_specular() const
  {
    return _transmittance_refraction_specular;
  }

private:
  texture _scattered_fraction;
  texture _emitted_light;
  texture _transmittance_refraction_specular;
};

namespace device_object {

class material_db
{
public:
  using material_map_id = std::size_t;

  material_db(const qcl::const_device_context_ptr& ctx)
  : _ctx{ctx}
  {}

  /// Allocate a new material map - prepares a fresh material_db if \c purge_host_memory()
  /// has been called. 
  material_map_id allocate_material_map(std::size_t width, std::size_t height)
  {
    std::size_t num_texels = width * height;
    _host_scattered_fraction.resize(_host_scattered_fraction.size() + num_texels);
    _host_emitted_light.resize(_host_emitted_light.size() + num_texels);
    _host_transmittance_refraction_specular.resize(
                _host_transmittance_refraction_specular.size() + num_texels);

    _host_widths.push_back(static_cast<cl_int>(width));
    _host_heights.push_back(static_cast<cl_int>(height));
    if(!_host_offsets.empty())
      _host_offsets.push_back(_host_offsets.back() + num_texels);
    else
      _host_offsets.push_back(0);

    _num_material_maps = _host_offsets.size();
  }

  material_map get_material_map(material_map_id index)
  {
    assert(index < static_cast<std::size_t>(_num_material_maps));

    cl_ulong offset = _host_offsets[index];
    return material_map(_host_scattered_fraction.data() + offset, 
                        _host_emitted_light.data() + offset,
                        _host_transmittance_refraction_specular.data() + offset,
                        static_cast<std::size_t>(_host_widths[index]),
                        static_cast<std::size_t>(_host_heights[index]));
  }

  int get_num_material_maps() const
  {
    return _num_material_maps;
  }

  const cl::Buffer& get_scattered_fraction_map() const
  {
    return _scattered_fraction;
  }
  
  const cl::Buffer& get_emitted_light() const
  {
    return _emitted_light;
  }

  const cl::Buffer& get_transmittance_refraction_specular() const
  {
    return _transmittance_refraction_specular;
  }

  const cl::Buffer& get_widths() const
  {
    return _width;
  }

  const cl::Buffer& get_height() const
  {
    return _height;
  }

  const cl::Buffer& get_offsets() const
  {
    return _offsets;
  }

  void commit_changes()
  {
    // Resize buffers and perform full data transfer
    _ctx->create_input_buffer<float4>(_scattered_fraction,
                                      _host_scattered_fraction.size(),
                                      _host_scattered_fraction.data());
    _ctx->create_input_buffer<float4>(_emitted_light,
                                      _host_emitted_light.size(),
                                      _host_emitted_light.data());
    _ctx->create_input_buffer<float4>(_transmittance_refraction_specular,
                                      _host_transmittance_refraction_specular.size(),
                                      _host_transmittance_refraction_specular.data());
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
    _host_scattered_fraction.clear();
    _host_emitted_light.clear();
    _host_transmittance_refraction_specular.clear();
    _host_heights.clear();
    _host_widths.clear();
    _host_offsets.clear();
  }

private:

  std::vector<float4> _host_scattered_fraction;
  std::vector<float4> _host_emitted_light;
  std::vector<float4> _host_transmittance_refraction_specular;
  std::vector<cl_int> _host_widths;
  std::vector<cl_int> _host_heights;
  std::vector<cl_ulong> _host_offsets;

  cl::Buffer _scattered_fraction;
  cl::Buffer _emitted_light;
  cl::Buffer _transmittance_refraction_specular;
  cl::Buffer _width;
  cl::Buffer _height;
  int _num_material_maps;
  cl::Buffer _offsets;

  qcl::const_device_context_ptr _ctx;
};


}
}

#endif
