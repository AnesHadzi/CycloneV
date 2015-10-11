/*
 * "Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It runs with or without the MicroC/OS-II RTOS and requires a STDOUT
 * device in your system's hardware.
 * The memory footprint of this hosted application is ~69 kbytes by default
 * using the standard reference design.
 *
 * For a reduced footprint version of this template, and an explanation of how
 * to reduce the memory footprint for a given application, see the
 * "small_hello_world" template.
 *
 */

#include <stdio.h>
#include <system.h>
#include <sys/alt_irq.h>
#include <inttypes.h>
#include "altera_avalon_mailbox_simple.h"

#define FRAME_OFFSET_FROM_ARM	0x03000000
#define BMP_HEADER_OFFSET		54
#define BMP_MAX					255

altera_avalon_mailbox_dev *mailbox_rx_dev;
altera_avalon_mailbox_dev *mailbox_tx_dev;


struct image_data {
	char name[20];
	char format[10];
	alt_u32 sdram_address;
	alt_u32 size;
	/* Pointer to image data */
	void *image_data;
};

static struct image_data image_dscr = {
	.name = "Image from ARM",
	.format = ".bmp",
	.sdram_address = FRAME_OFFSET_FROM_ARM,
	.size = 0,
	.image_data = NULL
};


static void process_in_place(void)
{
	image_dscr.image_data += BMP_HEADER_OFFSET;

	while ((alt_u32)image_dscr.image_data  < FRAME_OFFSET_FROM_ARM + image_dscr.size)
		*(volatile unsigned char *)(image_dscr.image_data++) = BMP_MAX -
		*(volatile unsigned char *)image_dscr.image_data;

}

static void mailbox_rx_isr(void *message)
{
	image_dscr.size = *(alt_u32 *)message;
	image_dscr.image_data = (void *)image_dscr.sdram_address;
	printf("Message from HPS: Picture placed at 0x0%x\n size: %d\n",
			(unsigned int)image_dscr.sdram_address, (unsigned int)image_dscr.size);

	process_in_place();

	/* Return the same message as ack, and start processing the image */
	printf("Message to HPS: %d\n", (unsigned int)image_dscr.size);
	/* Send message to HPS */
	altera_avalon_mailbox_send(mailbox_tx_dev, &image_dscr.size, 0, ISR);
}

int main()
{
   printf("Cao zdravo pera Nios II!\n");
   printf("Cao zdravo pera Nios II!\n");

   mailbox_rx_dev = altera_avalon_mailbox_open(MAILBOX_ARM2NIOS_NAME, NULL, mailbox_rx_isr);
   mailbox_tx_dev = altera_avalon_mailbox_open(MAILBOX_NIOS2ARM_NAME, NULL, NULL);


  while (1)
	  ;
  return 0;
}
