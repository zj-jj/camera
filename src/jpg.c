#include "jpg.h"




void show_jpg(char *fbmem, struct fb_var_screeninfo *pvinfo, char *jpg_buffer, unsigned long image_size)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	
	jpeg_mem_src(&cinfo, jpg_buffer, image_size);
	
	int ret = jpeg_read_header(&cinfo, true);
	if(ret != 1)
	{
		fprintf(stderr, "jpeg_read_header failed: %s\n", strerror(errno));
		exit(1);
	}
	jpeg_start_decompress(&cinfo);
	
	struct image_info *image_info = calloc(1, sizeof(struct image_info));
	if(image_info == NULL)
	{
		printf("malloc image_info failed\n");
	}	

	image_info->width = cinfo.output_width;
	image_info->height = cinfo.output_height;
	image_info->pixel_size = cinfo.output_components;
	
	printf("width:%d\theight:%d\n", image_info->width, image_info->height);
	
	int row_stride = image_info->width * image_info->pixel_size;

	unsigned long rgb_size;
	unsigned char *rgb_buffer;
	rgb_size = image_info->width * image_info->height * image_info->pixel_size;

	rgb_buffer = calloc(1, rgb_size);

	while(cinfo.output_scanline < cinfo.output_height)
	{
		unsigned char *buffer_array[1];
		buffer_array[0] = rgb_buffer + (cinfo.output_scanline) * row_stride;
		jpeg_read_scanlines(&cinfo, buffer_array, 1);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	
	int x, y;
	for(y=0; y<image_info->height && y<pvinfo->yres; y++)
	{
		for(x=0; x<image_info->width && x<pvinfo->xres; x++)
		{
			int image_offset = x * 3 + y * image_info->width * 3;
			int lcd_offset   = x * 4 + y * pvinfo->xres * 4;

			memcpy(fbmem+lcd_offset+0, rgb_buffer+image_offset+2, 1);
			memcpy(fbmem+lcd_offset+1, rgb_buffer+image_offset+1, 1);
			memcpy(fbmem+lcd_offset+2, rgb_buffer+image_offset+0, 1);
		}
	}
	
}
