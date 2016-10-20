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

#ifndef REDUCTION_CL
#define REDUCTION_CL

__constant sampler_t pixel_sampler = CLK_NORMALIZED_COORDS_FALSE | 
                                     CLK_ADDRESS_CLAMP_TO_EDGE |
                                     CLK_FILTER_NEAREST;

__kernel void max_value_reduction_init(__read_only image2d_t img,
                                       __global float* output)
{
  
  int width = get_image_width(img);
  int height = get_image_height(img);

  int px_x = get_global_id(0);
  int px_y = get_global_id(1);

  int buffer_pos = px_x * get_global_size(1) + px_y;

  float max_val = 0.0f;

  if(px_x < width && px_y < height)
  {
    int2 coord = (int2)(px_x, px_y);
    float4 px_color = read_imagef(img, pixel_sampler, coord);

    max_val = fmax(px_color.x, fmax(px_color.y, px_color.z));
  }
  output[buffer_pos] = max_val;
}

__kernel void max_value_reduction(__global float* buffer,
                                  __local float* partial)
{
  int local_id = get_local_id(0);
  int group_size = get_local_size(0);

  partial[local_id] = buffer[get_global_id(0)];
  barrier(CLK_LOCAL_MEM_FENCE);

  for (int i = group_size / 2; i > 0; i >>= 1)
  {
    if(local_id < i)
    {
      partial[local_id] = fmax(partial[local_id], partial[local_id + i]);
    }
    barrier(CLK_LOCAL_MEM_FENCE);
  }

  if(local_id == 0)
    buffer[get_group_id(0)] = partial[0];
}

#endif