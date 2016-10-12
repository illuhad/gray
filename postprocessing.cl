#ifndef POSTPROCESSING_CL
#define POSTPROCESSING_CL

#include "common.cl_hpp"

__constant sampler_t pixel_sampler = CLK_NORMALIZED_COORDS_FALSE | 
                                     CLK_ADDRESS_CLAMP_TO_EDGE |
                                     CLK_FILTER_NEAREST;

scalar color_post_process(scalar color, scalar max)
{
  scalar gamma = 1.f;
  return pow(clamp(color / max, 0.0f, 1.0f), gamma);
}

__kernel void hdr_color_compression(__write_only image2d_t output_pixels,
                                    __read_only image2d_t render_result,
                                    __global float* max_pixel_values)
{

  int width = get_image_width(output_pixels);
  int height = get_image_height(output_pixels);

  // Determine which pixel this thread will process
  int px_x = get_global_id(0);
  int px_y = get_global_id(1);

  float max_value = max_pixel_values[0];
  if(max_value < 0.2f)
    max_value = 0.2f;

  if(px_x == 512 && px_y == 512)
    printf("max = %f\n", max_value);

  if(px_x < width && px_y < height)
  {
    int2 coord = (int2)(px_x, px_y);
    float4 result_pixel = read_imagef(render_result, pixel_sampler, coord);
    result_pixel.x = color_post_process(result_pixel.x, max_value);
    result_pixel.y = color_post_process(result_pixel.y, max_value);
    result_pixel.z = color_post_process(result_pixel.z, max_value);
    result_pixel.w = 1.0f;

    write_imagef(output_pixels, coord, result_pixel);
  }
}

#endif