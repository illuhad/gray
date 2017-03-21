/*
 * This file is part of gray, a free, GPU accelerated, realtime pathtracing
 * engine,
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

#include "image.hpp"
#include "material_map.hpp"

#include <iostream>
#include <png++/png.hpp>

namespace gray {

void image::save_png(const std::string& filename,
                     const std::vector<unsigned char>& pixels,
                     std::size_t npx_x, std::size_t npx_y,
                     unsigned num_channels_per_pix, bool inverse_y_axis)
{
  png::image<png::rgb_pixel> image(static_cast<png::uint_32>(npx_x),
                                   static_cast<png::uint_32>(npx_y));

  for (std::size_t i = 0; i < pixels.size() / num_channels_per_pix; ++i)
  {
    unsigned char r = pixels[num_channels_per_pix * i];
    unsigned char g = pixels[num_channels_per_pix * i + 1];
    unsigned char b = pixels[num_channels_per_pix * i + 2];

    std::size_t x = i % npx_x;

    assert(i >= x);
    std::size_t y = 0;
    if (inverse_y_axis)
      y = npx_y - 1 - (i - x) / npx_x;
    else
      y = (i - x) / npx_x;

    image[y][x] = png::rgb_pixel(r, g, b);
  }

  image.write(filename.c_str());
}

void image::save_png(const std::string& filename,
                     const std::vector<unsigned char>& pixels,
                     std::size_t npx_x, std::size_t npx_y)
{
  save_png(filename, pixels, npx_x, npx_y, 3, true);
}

void image::save_png(const std::string& filename,
                     const qcl::device_context_ptr& ctx, const cl::Image2D& img,
                     std::size_t width, std::size_t height)
{
  // The dimension of the vector has to be 4 * width * height, because we
  // use RGBA and not RGB images.
  std::vector<unsigned char> pixels(4 * width * height);

  ctx->get_command_queue().enqueueReadImage(
      img, CL_TRUE, {{0, 0, 0}}, {{width, height, 1}}, 0, 0, pixels.data());

  save_png(filename, pixels, width, height, 4, false);
}

void image::initialize(int argc, char** argv)
{
  Magick::InitializeMagick(*argv);
}

image::image() : _width{0}, _height{0} {}

image::image(const std::string& file_name) { this->load(file_name); }

void image::load(const std::string& image_file_name)
{
  _img.read(image_file_name);
  _height = _img.rows();
  _width = _img.columns();
}

texture_id image::to_texture(device_object::material_db& materials) const
{
  texture_id tex = materials.allocate_texture(_width, _height);
  texture_accessor accessor = materials.access_texture(tex);

  for (std::size_t x = 0; x < _width; ++x)
    for (std::size_t y = 0; y < _height; ++y)
    {
      Magick::ColorRGB rgb(
          _img.pixelColor(static_cast<long>(x), static_cast<long>(y)));

      float4 color;
      color.s[0] = static_cast<scalar>(rgb.red());
      color.s[1] = static_cast<scalar>(rgb.green());
      color.s[2] = static_cast<scalar>(rgb.blue());
      color.s[3] = 1.0f;
      accessor.write(color, x, y);
    }
  return tex;
}
}
