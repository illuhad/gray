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
 
#include <vector>
#include <string>

#include <Magick++.h>


namespace gray {

void save_png(const std::string& filename, 
              const std::vector<unsigned char>& pixels,
              std::size_t npx_x,
              std::size_t npx_y);

class texture;

class image
{
public:
  image();
  image(const std::string &file_name);

  static void initialize(int argc, char **argv);

  void load(const std::string &image_file_name);

  void write_texture(texture &tex) const;

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
  std::size_t _width;
  std::size_t _height;

  Magick::Image _img;
};
}