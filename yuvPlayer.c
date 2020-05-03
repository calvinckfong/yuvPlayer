#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <assert.h>
#include <SDL2/SDL_pixels.h>

#define CLIP(x,_min,_max) ((x)<(_min))?(_min):(((x)>(_max))?(_max):(x))

int cam_fd;
void *z_sock;
sem_t picNum;
char *fileName = NULL;
int scale = 1;
int srcWidth = 0;
int srcHeight = 0;
int srcSize = 0;
int dstSize =0;
char *yuvFormat = NULL;
int pixFormat = 0;
char *srcdata = NULL;
char *dstdata = NULL;
int run=1;

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef void(*convertFnc)(const char* src, char* dst, int width, int height);
convertFnc convert;

extern void *thread_show(void *);

enum SourceType {
	INVALID = 0,
	RGB = 1,
	YUV420,
	YUV422,
	YUV444,
	YUV420_10,
	YUV422_10,
} ;


// functions converting to RGB24
void byPass(const char* src, char* dst, int width, int height)
{
	memcpy(dst, src, width*height*3);
}

void yuv420ToRgb(const char* src, char* dst, int width, int height)
{
	uint8_t *y0, *y1, *u, *v;
	uint8_t *dst0, *dst1;
	uint32_t planesize = width*height;
	uint32_t uvplaneSize = (width/2)*(height/2);
	int i, j;

	for (i=0; i<height; i+=2)
	{
		 y0 = (uint8_t*)(src + i*width);
		 y1 = (uint8_t*)(src + (i+1)*width);
		 u = (uint8_t*)(src + planesize + (i/2)*(width/2));
		 v = u + uvplaneSize;

		 dst0 = (uint8_t*)(dst + i*width*3);
		 dst1 = (uint8_t*)(dst + (i+1)*width*3);

		 for (j=0; j<width; j+=2)
		 {
			 float y0_value = *y0 - 16.0;
			 float y1_value = *y1 - 16.0;
			 float u_value = *u - 128.0;
			 float v_value = *v - 128.0;

			 dst0[0] = (uint8_t)CLIP((y0_value                   + 1.28033*v_value), 0, 255);
			 dst0[1] = (uint8_t)CLIP((y0_value - 0.21482*u_value - 0.38059*v_value), 0, 255);
			 dst0[2] = (uint8_t)CLIP((y0_value + 2.12798*u_value ), 0, 255);
			 dst0+=3; y0++;

			 dst1[0] = (uint8_t)CLIP((y1_value                   + 1.28033*v_value), 0, 255);
			 dst1[1] = (uint8_t)CLIP((y1_value - 0.21482*u_value - 0.38059*v_value), 0, 255);
			 dst1[2] = (uint8_t)CLIP((y1_value + 2.12798*u_value ), 0, 255);
			 dst1+=3; y1++;

			 y0_value = *y0 - 16.0;
			 y1_value = *y1 - 16.0;

			 dst0[0] = (uint8_t)CLIP((y0_value                   + 1.28033*v_value), 0, 255);
			 dst0[1] = (uint8_t)CLIP((y0_value - 0.21482*u_value - 0.38059*v_value), 0, 255);
			 dst0[2] = (uint8_t)CLIP((y0_value + 2.12798*u_value ), 0, 255);
			 dst0+=3; y0++;

			 dst1[0] = (uint8_t)CLIP((y1_value                   + 1.28033*v_value), 0, 255);
			 dst1[1] = (uint8_t)CLIP((y1_value - 0.21482*u_value - 0.38059*v_value), 0, 255);
			 dst1[2] = (uint8_t)CLIP((y1_value + 2.12798*u_value ), 0, 255);
			 dst1+=3; y1++;
			 u++; v++;
		 }
	}

}

int main(int argc, char *argv[]) 
{
	pthread_t a;
	int ret = 0;
	int fd = 0;
	int i, j;
	int srcType = INVALID;

	if(argc != 5) {
	    printf("Usage: \n"
		   "\t%s File Width Height Format[rgb|yuv420]\n"
		   "\te.g.: %s inputfile.rgb 1280 720 rgb\n", argv[0], argv[0]);
	    return -1;
	  }

	// init
	fileName = argv[1];
	srcWidth = atoi(argv[2]);
	srcHeight = atoi(argv[3]);

	yuvFormat = argv[4];
	printf("FILENAME:\t%s\n"
		"WIDTH:\t%d\n"
		"HEIGHT:\t%d\n"
		"FORMAT:\t%s\n",
		fileName, srcWidth, srcHeight, yuvFormat);

	dstSize = srcWidth * srcHeight * 3;
	assert(NULL != (dstdata=(char*)malloc(dstSize)));
	pixFormat = SDL_PIXELFORMAT_RGB24;

	if (!strcmp(yuvFormat, "rgb"))
	{
		srcType = RGB;
		srcSize = srcWidth * srcHeight * 3;
		assert(NULL != (srcdata=(char*)malloc(srcSize)));
		convert = byPass;
	}
	else if (!strcmp(yuvFormat, "yuv420"))
	{
		srcType = YUV420;
		srcSize = srcWidth * srcHeight * 3 / 2;
		assert(NULL != (srcdata=(char*)malloc(srcSize)));
		convert = yuv420ToRgb;
	}
	else
	{
		printf("pix format %s not support currently!\n", yuvFormat);
		return -1;
	}

	fd = open(fileName, O_RDONLY);
	assert(fd>0);

	sem_init(&picNum, 0, 0);
	pthread_create(&a, NULL, thread_show, NULL);
	while(run) {
		if (read(fd, srcdata, srcSize) <=0)
		{
			printf("End of file. Press q to quit.\n");
			usleep(1*1000*1000);
		}
		else
		{
			convert(srcdata, dstdata, srcWidth, srcHeight);
			sem_post(&picNum);
			usleep(33*1000);
		}
	}
	close(fd);
	free(srcdata);
	free(dstdata);

	return 0;
}


