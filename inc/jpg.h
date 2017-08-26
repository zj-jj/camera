#ifndef __JPG_H
#define __JPG_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <linux/input.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "jpeglib.h"
#include <linux/fb.h>

struct image_info
{
	int width;
	int height;
	int pixel_size;
};


void show_jpg(char *fbmem, struct fb_var_screeninfo *pvinfo, char *jpg_buffer, unsigned long image_size);

#endif

