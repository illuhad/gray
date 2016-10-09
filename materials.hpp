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

  inline static
  void create_uniform_material(material_map material,
                              rgb_color scattered_fraction,
                              rgb_color emitted_light,
                              scalar transmittance,
                              scalar refraction_index,
                              scalar specular)
  {
    texture &scattered_fraction_map = material.get_scattered_fraction();
    texture &emitted_light_map = material.get_emitted_light();
    texture &transmittance_refraction_specular_map = 
          material.get_transmittance_refraction_specular();

    for (std::size_t x = 0; x < scattered_fraction_map.get_width(); ++x)
    {
      for(std::size_t y = 0; y < scattered_fraction_map.get_height(); ++y)
      {
        rgba_color scattered = embed_rgb_in_rgba(scattered_fraction, 0.0f);
        rgba_color emitted = embed_rgb_in_rgba(emitted_light, 0.0f);
        rgba_color additional_properties = {{transmittance, refraction_index, specular, 0.0f}};

        scattered_fraction_map.write(scattered, x, y);
        emitted_light_map.write(emitted, x, y);
        transmittance_refraction_specular_map.write(additional_properties, x, y);
      }
    }
  }

  inline static
  void create_uniform_material(material_map material,
                              rgb_color scattered_fraction,
                              scalar transmittance,
                              scalar refraction_index,
                              scalar specular)
  {
    create_uniform_material(material, 
                            scattered_fraction, 
                            {{0.0f, 0.0f, 0.0f, 0.0f}}, 
                            transmittance, refraction_index, specular);
  }

  inline static
  void create_uniform_emissive_material(material_map material,
                                        rgb_color emission)
  {
    create_uniform_material(material,
                            {{0.0f, 0.0f, 0.0f, 0.0f}},
                            embed_rgb_in_rgba(emission, 0.0f),
                            0.0,
                            1.0,
                            1.0);
  }

  inline 
  void create_uniform_material(device_object::material_db::material_map_id id,
                              rgb_color scattered_fraction,
                              rgb_color emitted_light,
                              scalar transmittance,
                              scalar refraction_index,
                              scalar specular)
  {
    create_uniform_material(_materials->get_material_map(id),
                            scattered_fraction, emitted_light, 
                            transmittance, refraction_index, specular);
  }

  inline 
  void create_uniform_material(device_object::material_db::material_map_id id,
                              rgb_color scattered_fraction,
                              scalar transmittance,
                              scalar refraction_index,
                              scalar specular)
  {
    create_uniform_material(_materials->get_material_map(id),
                            scattered_fraction, 
                            transmittance, refraction_index, specular);
  }


 inline
 void create_uniform_emissive_material(device_object::material_db::material_map_id id,
                                        rgb_color emission)
  {
    create_uniform_emissive_material(_materials->get_material_map(id),
                                     emission);
  }
private:
  device_object::material_db *_materials;
};
}

#endif