#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>		/* memcpy */

#include "hwlib.h"
#include "soc_cv_av/socal/socal.h"
#include "soc_cv_av/socal/hps.h"
#include "soc_cv_av/socal/alt_gpio.h"
//#include "hps_0.h"


/* Defines for communication with mailbox driver */
#define DRIVER_NAME				"/dev/mailbox-nios"
#define MAJOR_NUMBER			255
#define COMMUNICATION_ERROR		-101

#define SET_IMAGE_STATUS	_IOWR(MAJOR_NUMBER, 1, unsigned long)
#define GET_IMAGE_STATUS	_IOWR(MAJOR_NUMBER, 2, unsigned long)

/* Reigisters offsets */
#define HW_REGS_SPAN		0x04000000
#define HW_REGS_MASK		HW_REGS_SPAN - 1

#define HPS2FPGA_BASE		0xC0000000
#define FPGA_SDRAM_BASE		0x0
#define SDRAM_FRAME_OFFSET	0x03000000


/**
 * @brief communication between driver
 * @param
 *
 * @return COMMUNICATION STATUS
 * 
 */
static void set_image_status(unsigned long size)
{
	int fd;
	int ret = 0;

	
	fd = open(DRIVER_NAME, O_RDWR);
	if (fd == -1) {
		printf("ERROR: Couldn't open file!\n");
		return;
	}

	ret = ioctl(fd, SET_IMAGE_STATUS, &size);
	if (ret == -1) {
		printf("ERROR: Couldn't notify Communication driver!!\n");
		close (fd);
		return;
	}

	close (fd);
	return;
}

static int get_image_status(void)
{
	int fd;
	int ret = 0;
	unsigned long size;

	fd = open(DRIVER_NAME, O_RDWR);
	if (fd == -1) {
		printf("ERROR: Couldn't open file!\n");
		return COMMUNICATION_ERROR;
	}

	ret = ioctl(fd, GET_IMAGE_STATUS, &size);
	if (ret == -1) {
		printf("ERROR: Couldn't notify Communication driver!!\n");
		close (fd);
		return COMMUNICATION_ERROR;
	}

	close (fd);

	return size;
}

int main(int argc, char **argv) {

	int fd, pic_fd, copy_fd;
	struct stat pic_mem_stat;
	void *h2p_bridge_addr, *pic_addr, *copy_addr;
	int ret;

	if (argc < 2) {
		printf("Err, Please specify the image\n");
		return 1;
	}

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if(fd == -1) {
		printf( "ERROR: could not open \"/dev/mem\"...\n" );
		return 1;
	}

	h2p_bridge_addr = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE,
							MAP_SHARED, fd, HPS2FPGA_BASE + SDRAM_FRAME_OFFSET);

	if (h2p_bridge_addr == MAP_FAILED) {
		printf("ERROR: mmap() failed..\n");
		close(fd);
		return 1;
	}

	pic_fd = open(argv[1], O_RDWR);
	if (pic_fd == -1) {
		printf("%s, failed to open picture\n", __func__);
		return 1;
	}

	ret = fstat(pic_fd, &pic_mem_stat);
	if (ret < 0){
		printf("%s fstat error\n", __func__);
		close(pic_fd);
		return ret;
	}

	/* Create receiver for picture from Nios */
	fclose(fopen("fromnios.bmp", "wb"));
	copy_fd = open("fromnios.bmp", O_RDWR);
	if (copy_fd == -1) {
		printf("%s failed to open image\n", __func__);
		return 1;
	}

	/* Map picture to memory */
	pic_addr = mmap(NULL, pic_mem_stat.st_size, PROT_READ | PROT_WRITE,
					MAP_SHARED, pic_fd, 0);
	if (pic_addr == MAP_FAILED) {
		printf("%s failed to map picture to memory\n", __func__);
		return 1;
	}
	
	copy_addr = mmap(NULL, pic_mem_stat.st_size, PROT_READ | PROT_WRITE,
					MAP_SHARED, copy_fd, 0);
	if (copy_addr == MAP_FAILED) {
		printf("%s failed to map picture to memory\n", __func__);
		return 1;
	}

	printf("Starting transfer\n address: 0x%x\n", (uint32_t)h2p_bridge_addr);
	ftruncate(fd, pic_mem_stat.st_size);
	ftruncate(copy_fd, pic_mem_stat.st_size);

	/*Copy image to Nios SDRAM*/
	memcpy(h2p_bridge_addr, pic_addr, pic_mem_stat.st_size);
	set_image_status(pic_mem_stat.st_size);

	/* Wait to Nios process the image */
	while (get_image_status() == 0)
		;

	/* Copy image from Nios */
	memcpy(copy_addr, h2p_bridge_addr, pic_mem_stat.st_size);


	/* clean up our memory mapping and exit */
	ret = munmap(h2p_bridge_addr, HW_REGS_SPAN);
	if(ret != 0) {
		printf("ERROR: munmap() failed...\n");
		close(fd);
		return ret;
	}
	
	ret = munmap( pic_addr, pic_mem_stat.st_size );
	if(ret != 0) {
		printf("ERROR: pic munmap() failed...\n" );
		close(fd);
		return ret;
	}

	close(fd);

	return 0;
}
