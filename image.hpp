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

#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <string>
#include <vector>

#include <Magick++.h>

#include "material_map.hpp"
#include "qcl.hpp"

namespace cl {
class Image2D;
}

namespace gray {

class texture_accessor;

class image
{
public:
  image();
  image(const std::string& file_name);

  static void initialize(int argc, char** argv);

  void load(const std::string& image_file_name);

  texture_id to_texture(device_object::material_db& materials) const;

  inline std::size_t get_width() const { return _width; }

  inline std::size_t get_height() const { return _height; }

  static void save_png(const std::string& filename,
                       const std::vector<unsigned char>& pixels,
                       std::size_t npx_x, std::size_t npx_y);

  static void save_png(const std::string& filename,
                       const qcl::device_context_ptr& ctx,
                       const cl::Image2D& img, std::size_t width,
                       std::size_t height);

private:
  static void save_png(const std::string& filename,
                       const std::vector<unsigned char>& pixels,
                       std::size_t npx_x, std::size_t npx_y,
                       unsigned num_channels_per_pix, bool inverse_y_axis);

  std::size_t _width;
  std::size_t _height;

  Magick::Image _img;
};
}

#endif
