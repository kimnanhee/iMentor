#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "videodev2.h"
#include "hdmi_api.h"
#include "hdmi_lib.h"
#include "s3c_lcd.h"

#include "bmp.h"

#define FB_DEV	"/dev/fb0"

typedef struct FrameBuffer {
	int         fd;
	void        *start;
	size_t      length;
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
} FrameBuffer;


// 키보드 이벤트를 처리하기 위한 함수, Non-Blocking 입력을 지원
//  값이 없으면 0을 있으면 해당 Char값을 리턴
static int kbhit(void)
{
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if(ch != EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}

	return 0;
}

int fb_open(FrameBuffer *fb){
	int fd;
	int ret;

	fd = open(FB_DEV, O_RDWR);
	if(fd < 0){
		perror("FB Open");
		return -1;
	}
	ret = ioctl(fd, FBIOGET_FSCREENINFO, &fb->fix);
	if(ret < 0){
		perror("FB ioctl");
		close(fd);
		return -1;
	}
	ret = ioctl(fd, FBIOGET_VSCREENINFO, &fb->var);
	if(ret < 0){
		perror("FB ioctl");
		close(fd);
		return -1;
	}
	fb->start = (unsigned char *)mmap (0, fb->fix.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if(fb->start == NULL){
		perror("FB mmap");
		close(fd);
		return -1;
	}
	fb->length = fb->fix.smem_len;
	fb->fd = fd;
	return fd;
}

void fb_close(FrameBuffer *fb){
	if(fb->fd > 0)
		close(fb->fd);
	if(fb->start > 0){
		msync(fb->start, fb->length, MS_INVALIDATE | MS_SYNC);
		munmap(fb->start, fb->length);
	}
}

int main()
{
	int i, j;
	int x, y;
	int ret;
	unsigned int *pos;
	int endFlag = 0;
	int ch;
	unsigned int phyLCDAddr = 0;
	int bmpWidth, bmpHeight;
	unsigned int *bitmap= 0;

	sec_g2d_t g2d;
	g2d_rect src_rect, dst_rect;
	g2d_flag g2dflag;

	FrameBuffer gfb;

	printf("Bitblt Test Program Start\n");

	ret = fb_open(&gfb);
	if(ret < 0){
		printf("Framebuffer open error");
		perror("");
		return -1;
	}

	// get physical framebuffer address for LCD
	if (ioctl(ret, S3CFB_GET_LCD_ADDR, &phyLCDAddr) == -1)
	{
		printf("%s:ioctl(S3CFB_GET_LCD_ADDR) fail\n", __func__);
		return 0;
	}
	printf("phyLCD:%x\n", phyLCDAddr);

	hdmi_initialize();

	hdmi_gl_initialize(0);
	hdmi_gl_set_param(0, phyLCDAddr, 1280, 720, 0, 0, 0, 0, 1);
	hdmi_gl_streamon(0);

	//============= G2D ==================
	memset(&g2d, 0x00, sizeof(sec_g2d_t));
	createG2D(&g2d);
	//====================================

	pos = (unsigned int*)gfb.start;
	bitmap = &pos[1280*720];

	memset(pos, 0x00, 1280*720*4);

	if ( fh_bmp_getsize("test.bmp", &bmpWidth, &bmpHeight) == FH_ERROR_OK )
	{
		printf("test.bmp size is %dx%d\n", bmpWidth, bmpHeight);

		fh_bmp_load("test.bmp", bitmap, bmpWidth, bmpHeight, 0xFF);
	}
	printf("BMP Loading Done!\n" );

	//============= G2D ==================
	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.w = 1280;
	src_rect.h = 720;
	src_rect.full_w = 1280;
	src_rect.full_h = 720;
	src_rect.color_format = G2D_ARGB_8888;
	src_rect.phys_addr = phyLCDAddr+1280*720*4;

	dst_rect.x = 50;
	dst_rect.y = 50;
	dst_rect.w = 640;
	dst_rect.h = 320;
	dst_rect.full_w = 1280;
	dst_rect.full_h = 720;
	dst_rect.color_format = G2D_ARGB_8888;
	dst_rect.phys_addr = phyLCDAddr;

	memset(&g2dflag, 0x00, sizeof(g2d_flag));

	doG2D(&g2d, &src_rect, &dst_rect, &g2dflag); // 원 이미지의 정보, 목적지 이미지 정보

	dst_rect.x = 640;
	dst_rect.y = 320;
	dst_rect.w = 160;
	dst_rect.h = 240;
	g2dflag.rotate_val = G2D_ROT_90;
	doG2D(&g2d, &src_rect, &dst_rect, &g2dflag);

	dst_rect.x = 20;
	dst_rect.y = 320;
	dst_rect.w = 180;
	dst_rect.h = 120;
	g2dflag.rotate_val = G2D_ROT_180;
	doG2D(&g2d, &src_rect, &dst_rect, &g2dflag);
	//====================================

	printf("'q' is Quit\n");
	while (!endFlag)
	{
		usleep(10*1000);

		if (kbhit())
		{
			ch = getchar();
			switch ( ch )
			{
				case 'q': endFlag = 1;
					break;
			}
		}
	}


	destroyG2D(&g2d);
	hdmi_gl_streamoff(0);
	hdmi_gl_deinitialize(0);
	hdmi_deinitialize();
	fb_close(&gfb);

	return 0;
}


