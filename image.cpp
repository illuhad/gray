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

void save_png(const std::string& filename,
              const std::vector<unsigned char>& pixels, std::size_t npx_x,
              std::size_t npx_y)
{
  png::image<png::rgb_pixel> image(static_cast<png::uint_32>(npx_x),
                                   static_cast<png::uint_32>(npx_y));

  for (std::size_t i = 0; i < pixels.size() / 3; ++i)
  {
    unsigned char r = pixels[3 * i];
    unsigned char g = pixels[3 * i + 1];
    unsigned char b = pixels[3 * i + 2];

    std::size_t x = i % npx_x;

    assert(i >= x);
    std::size_t y = npx_y - 1 - (i - x) / npx_x;

    image[y][x] = png::rgb_pixel(r, g, b);
  }

  image.write(filename.c_str());
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

void image::write_texture(texture& tex) const
{
  assert(_height == tex.get_height());
  assert(_width == tex.get_width());
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
      tex.write(color, x, y);
    }
}
}
