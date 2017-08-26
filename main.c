#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <string.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/input.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include "jpeglib.h"

#include "jpg.h"

#define CAMERA_DEVICE "/dev/video7"
#define VIDEO_WEIGHT   640
#define VIDEO_HEIGHT   480
#define BUFFER_COUNT	4

#define JPG_FILE "1.jpg"

#define LCD_DEVICE "/dev/fb0"

typedef struct VideoBuffer
{
	void *start;
	size_t length;
}VideoBuffer;


int main(int argc, char **argv)
{
	int ret;
	
	// 1. 打开lcd
	int lcd_fd = open(LCD_DEVICE, O_RDWR);
	if(lcd_fd == -1)
	{
		perror("open lcd_device failed.");
		exit(EXIT_FAILURE);	
	}
	struct fb_var_screeninfo vinfo;
	ioctl(lcd_fd, FBIOGET_VSCREENINFO, &vinfo);
	unsigned long fb_size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel/8;	
		
	// 2.内存映射
	char *fbmem = mmap(NULL, fb_size, PROT_READ|PROT_WRITE, MAP_SHARED, lcd_fd, 0);
	if(fbmem == MAP_FAILED)
	{
		perror("mmap failed");
		exit(EXIT_FAILURE);
	}	
	
	// 3.打开camera
	int camera_fd = open(CAMERA_DEVICE, O_RDWR);
	if(camera_fd == -1)
	{
		perror("open camera_device failed");
		exit(EXIT_FAILURE);
	}	
	
	// 对摄像头处理
	
	// 1.获取驱动信息
	struct v4l2_capability cap;
	bzero(&cap, sizeof(cap));
	ret = ioctl(camera_fd, VIDIOC_QUERYCAP, &cap);
	if(ret < 0)
	{
		printf("ioctl failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
//#ifdef DEBUG	
	printf("Capability Informations:\n");
	printf("driver      : %s\n", cap.driver);
	printf("card  	    : %s\n", cap.card);
	printf("bus_info    : %s\n", cap.bus_info);
	printf("version     : %u\n", cap.version);
	printf("capabilities: %u\n", cap.capabilities);
//#endif	
	
	// 2.设置视频格式
	struct v4l2_format fmt;	
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width  = VIDEO_WEIGHT;
	fmt.fmt.pix.height = VIDEO_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
	ret = ioctl(camera_fd, VIDIOC_S_FMT, &fmt);
	if(ret < 0)
	{
		perror("VIDIOC_S_FMT failed");
		exit(EXIT_FAILURE);
	}
	
	// 3.获取视频格式
	ret = ioctl(camera_fd, VIDIOC_G_FMT, &fmt);
	if(ret < 0)
	{
		perror("VIDIOC_G_FMT failed");
		exit(EXIT_FAILURE);
	}
	printf("\n---------------------------\n");	
	printf("Stream Format Informations:\n");
	printf("type: %d\n", fmt.type);
	printf("width: %u\n", fmt.fmt.pix.width);
	printf("height: %u\n", fmt.fmt.pix.height);
	
	char fmtstr[8];
	memset(fmtstr,0, 8);
	memcpy(fmtstr, &fmt.fmt.pix.pixelformat, 8);
	printf("pixelformat: %s\n", fmtstr);
	
	// 4.配置内存映射
	struct v4l2_requestbuffers reqbuf;
	memset(&reqbuf, 0, sizeof(reqbuf));
	reqbuf.count = BUFFER_COUNT;
	reqbuf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory= V4L2_MEMORY_MMAP;
	
	ret = ioctl(camera_fd, VIDIOC_REQBUFS, &reqbuf); // VIDIOC_REQBUFS 分配内存
	if(ret < 0)
	{
		perror("VIDIOC_REQBUFS failed.");
		exit(EXIT_FAILURE);
	}
	
	// 5.获取空间
	VideoBuffer *buffers = calloc(reqbuf.count, sizeof(*buffers));
	struct v4l2_buffer buf;
	
	int i;
	for(i=0; i<reqbuf.count; i++)
	{
		memset(&buf, 0, sizeof(buf));
		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		
		ret = ioctl(camera_fd, VIDIOC_QUERYBUF, &buf); //把 VIDIOC_REQBUFS 中分配的数据缓存转换成物理地址 
		if(ret < 0)
		{
			perror("VIDIOC_QUERYBUF failed.");
			exit(EXIT_FAILURE);
		}

		buffers[i].length = buf.length;
		buffers[i].start  = mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, camera_fd, buf.m.offset);
		
		if(buffers[i].start == MAP_FAILED)
		{
			printf("mmap %d failed: %s\n", i, strerror(errno));
			exit(EXIT_FAILURE);
		}	

		ret = ioctl(camera_fd, VIDIOC_QBUF, &buf); // VIDIOC_QBUF 把数据从缓存中读取出来
		if(ret < 0)
		{
			perror("VIDIOC_QBUF failed");
			exit(EXIT_FAILURE);
		}	
	}	
	printf("frame buffer %d: address=0x%x, length=%d\n", i, (unsigned int)buffers[i].start, buffers[i].length);	
	
	// 6.开始采集
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;	
	ret = ioctl(camera_fd, VIDIOC_STREAMON, &type);
	if(ret < 0)
	{
		perror("VIDIOC_STREAMON failed");
		exit(EXIT_FAILURE);
	}	
	
	// 7. 读取缓冲区
	while(1)
	{	
		for(i=0; i<BUFFER_COUNT; i++)
		{	
			buf.index = i;
			ret = ioctl(camera_fd, VIDIOC_DQBUF, &buf); // VIDIOC_DQBUF 把数据放回缓存队列
			if(ret < 0)
			{
				perror("VIDIOC_DQBUF failed");
				exit(EXIT_FAILURE);
			}
			
			//show_jpg(buffers[buf.index].start, buffers[buf.index].length, fbmem. &vinfo);
			show_jpg(fbmem, &vinfo, buffers[buf.index].start, buffers[buf.index].length);
			
			
			ret = ioctl(camera_fd, VIDIOC_QBUF, &buf);
			if(ret < 0)
			{
				perror("VIDIOC_QBUF failed");
				exit(EXIT_FAILURE);
			}					
		}
	}
	
	/* 停止采集 */
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(camera_fd, VIDIOC_STREAMOFF, &type);
	if(ret < 0)
	{
		perror("VIDIOC_STREAMOFF failed");
		exit(EXIT_FAILURE);
	}
	
	/* 取消内存映射 */
	for(i=0; i<4; i++)
	{
		munmap(buffers[i].start, buffers[i].length);
	}
	
	close(camera_fd);
	
	// 解除lcd映射
	munmap(fbmem, fb_size);
	
	
	return 0;
}



