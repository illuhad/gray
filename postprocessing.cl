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

#ifndef POSTPROCESSING_CL
#define POSTPROCESSING_CL

#include "common.cl_hpp"

__constant sampler_t pixel_sampler = CLK_NORMALIZED_COORDS_FALSE | 
                                     CLK_ADDRESS_CLAMP_TO_EDGE |
                                     CLK_FILTER_NEAREST;

scalar color_post_process(scalar color, scalar max)
{
  scalar gamma = 1.f / 2.2f;
  return pow(clamp(color / max, 0.0f, 1.0f), gamma);
}

__kernel void hdr_color_compression(__write_only image2d_t output_pixels,
                                    __read_only image2d_t render_result,
                                    __global float* max_pixel_values,
                                    __global float* max_values_running_average,
                                    unsigned long frame_number,
                                    int smoothing_range)
{

  __local float running_average[MAX_VALUE_RUNNING_AVERAGE_SIZE];
  int local_id = get_local_id(0) * get_local_size(1) + get_local_id(1);
  int new_max_value_pos = frame_number % MAX_VALUE_RUNNING_AVERAGE_SIZE;

  // Load buffer for running average of max pixel value into local memory,
  // and update buffer in global memory.
  float max_value = max_pixel_values[0];
  if(max_value < 0.2f)
    max_value = 0.2f;

  if (local_id < MAX_VALUE_RUNNING_AVERAGE_SIZE)
  {
    if (local_id == new_max_value_pos)
    {
      running_average[local_id] = max_value;
    }
    else
      running_average[local_id] = max_values_running_average[local_id];
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  if(get_global_id(0) * get_global_size(1) + get_global_id(1) == new_max_value_pos)
    max_values_running_average[new_max_value_pos] = max_value;

  // Calculate running average
  float avg_max_value = 0.0f;
  // MAX_VALUE_RUNNING_AVERAGE_SIZE is usually so small that there are no
  // more sophisticated reduction mechanisms necessary.
  for (int i = 0; i < MAX_VALUE_RUNNING_AVERAGE_SIZE; ++i)
    avg_max_value += running_average[i];
  avg_max_value /= (float)MAX_VALUE_RUNNING_AVERAGE_SIZE;

  int width = get_image_width(output_pixels);
  int height = get_image_height(output_pixels);

  // Determine which pixel this thread will process
  int px_x = get_global_id(0);
  int px_y = get_global_id(1);


  // Load pixel for color post processing from image,
  // apply smoothing.
  float4 result_pixel = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
  int2 coord;

  int num_contributions = (2 * smoothing_range + 1) * (2 * smoothing_range + 1);
  int2 current_coord = (int2)(px_x - smoothing_range, px_y - smoothing_range);
  int2 max_coord = (int2)(px_x + smoothing_range, px_y + smoothing_range);

  float weight_sum = 0.0f;
  float scale_factor = 1.f / (8.f * smoothing_range * smoothing_range + 1.f);

  if (px_x < width && px_y < height)
  {
    for (; current_coord.x <= max_coord.x; ++current_coord.x)
    {
      for (current_coord.y = px_y - smoothing_range; current_coord.y <= max_coord.y; ++current_coord.y)
      {
        float dx = (float)(current_coord.x - px_x);
        float dy = (float)(current_coord.y - px_y);
        float weight = exp(-scale_factor*(dx * dx + dy*dy));
        result_pixel += weight * read_imagef(render_result, pixel_sampler, current_coord);
        weight_sum += weight;
      }
    }

    result_pixel *= 1.f / weight_sum;

    result_pixel.x = color_post_process(result_pixel.x, avg_max_value);
    result_pixel.y = color_post_process(result_pixel.y, avg_max_value);
    result_pixel.z = color_post_process(result_pixel.z, avg_max_value);
    result_pixel.w = 1.0f;
    coord = (int2)(px_x, px_y);

    write_imagef(output_pixels, coord, result_pixel);
  }
}

#endif