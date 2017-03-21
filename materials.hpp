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

#ifndef MATERIALS_HPP
#define MATERIALS_HPP

#include "material_map.hpp"

namespace gray {

class material_factory
{
public:
  material_factory(device_object::material_db* materials)
  : _materials{materials}
  {
    assert(materials);
  }

  material_factory(const material_factory& other) = default;
  material_factory &operator=(const material_factory &other) = default;

  inline
  texture_id create_uniform_scattered_fraction_texture(rgb_color scattered_fraction)
  {
    texture_id scattered_fraction_tex = _materials->allocate_texture(1,1);
    rgba_color scattered = embed_rgb_in_rgba(scattered_fraction, 0.0f);
    _materials->access_texture(scattered_fraction_tex).fill(scattered);

    return scattered_fraction_tex;
  }

  inline
  texture_id create_uniform_emission_texture(rgb_color emitted_light)
  {
    texture_id emitted_light_tex = _materials->allocate_texture(1,1);
    rgba_color emitted = embed_rgb_in_rgba(emitted_light, 0.0f);
    _materials->access_texture(emitted_light_tex).fill(emitted);

    return emitted_light_tex;
  }

  inline
  texture_id create_uniform_additional_properties_texture(scalar transmittance,
                                                          scalar refraction_index,
                                                          scalar roughness)
  {
    texture_id transmittance_refraction_roughness_tex = _materials->allocate_texture(1,1);
    rgba_color additional_properties = {{transmittance, refraction_index, roughness, 0.0f}};
    _materials->access_texture(transmittance_refraction_roughness_tex).fill(additional_properties);

    return transmittance_refraction_roughness_tex;
  }

  inline
  material_id create_background_material(texture_id background_texture)
  {
    return _materials->create_material(create_uniform_scattered_fraction_texture({{0.0f, 0.0f, 0.0f, 0.0f}}),
                                       background_texture,
                                       create_uniform_additional_properties_texture(0.0, 1.0, 1.0));
  }

  inline
  material_id create_uniform_material(rgb_color scattered_fraction,
                              rgb_color emitted_light,
                              scalar transmittance,
                              scalar refraction_index,
                              scalar roughness)
  {

    return _materials->create_material(create_uniform_scattered_fraction_texture(scattered_fraction),
                                create_uniform_emission_texture(emitted_light),
                                create_uniform_additional_properties_texture(transmittance, refraction_index, roughness));
  }

  inline
  material_id create_uniform_material(rgb_color scattered_fraction,
                              scalar transmittance,
                              scalar refraction_index,
                              scalar roughness)
  {
    return create_uniform_material(scattered_fraction,
                                  {{0.0f, 0.0f, 0.0f, 0.0f}},
                                   transmittance, refraction_index, roughness);
  }

  inline
  material_id create_uniform_emissive_material(rgb_color emission)
  {
    return create_uniform_material(
                            {{0.0f, 0.0f, 0.0f, 0.0f}},
                            embed_rgb_in_rgba(emission, 0.0f),
                            0.0,
                            1.0,
                            1.0);
  }


private:
  device_object::material_db *_materials;
};
}

#endif
