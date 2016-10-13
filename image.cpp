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

#include "image.hpp"  
#include <png++/png.hpp>
#include <iostream>  
  
void save_png(const std::string& filename, 
              const std::vector<unsigned char>& pixels,
              std::size_t npx_x,
              std::size_t npx_y)
{  
  png::image<png::rgb_pixel> image(npx_x, npx_y);
      
  for(std::size_t i = 0; i < pixels.size() / 3; ++i)
  {
    unsigned char r = pixels[3 * i    ];
    unsigned char g = pixels[3 * i + 1];
    unsigned char b = pixels[3 * i + 2];
    
    std::size_t x = i % npx_x;
    
    assert(i >= x);
    std::size_t y = npx_y - 1 - (i - x) / npx_x;
    
    image[y][x] = png::rgb_pixel(r,g,b);
   
  }
  
  image.write(filename.c_str());
} 
