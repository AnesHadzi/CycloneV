#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/io.h>
#include <linux/ioctl.h>

#include <asm/opcodes.h>
#include <linux/buffer_head.h>
#include <linux/device.h>

#define DRIVER_NAME		"mailbox-nios"
#define MAJOR_NUMBER	255

#define SET_IMAGE_STATUS	_IOWR(MAJOR_NUMBER, 1, unsigned long)
#define GET_IMAGE_STATUS	_IOWR(MAJOR_NUMBER, 2, unsigned long)
#define COMMUNICATION_ERROR		-101

#define HPS_FPGA_BASE		0xC0000000
#define ONCHIP_BASE		0x08000000
#define ONCHIP_SIZE		0x4000

#define HPS_DDR3_START	0x00100000
#define HPS_DDR3_END	0xC0000000

/* Process statuses */
#define PROCESS_START		0

static struct mbox_chan *chan_sender;
static struct mbox_chan *chan_receiver;
static struct mbox_client client_sender;
static struct mbox_client client_receiver;
volatile int process_finished = 0;

/* Comunication with user space */
static dev_t driver_dev = MKDEV(MAJOR_NUMBER, 1);
static struct cdev driver_cdev;
static struct file_operations driver_fops;
static int driver_count = 1;
static struct class *cl;

struct image_data {
	char name[20];
	uint32_t ddr3_address;
	uint32_t size;
	char format[10];
} image_dscr;

static void receiver(struct mbox_client *cl, void *mssg)
{
	process_finished = *(uint32_t *)mssg;
	pr_info("Message received from nios\nImage size: %d bytes\n", process_finished);
}

static const struct of_device_id mbox_nios_of_match[] = {
	{ .compatible = "client-1.0" },
	{},
};

MODULE_DEVICE_TABLE(of, mbox_nios_of_match);

static int get_process_status(void)
{
	return process_finished;
}

static void set_process_status(int status)
{
	process_finished = status;
}

static int  mailbox_nios_probe(struct platform_device *pdev)
{
	pr_info("Start: Mailbox to NIOS Init\n");
	/* Send message to NIOS II */
	client_sender.dev = &pdev->dev;
	
	/* Receive message from NIOS II */
	client_receiver.dev = &pdev->dev;
	client_receiver.rx_callback = receiver;
	
	chan_sender = mbox_request_channel_byname(&client_sender, "mbox-tx");
	if (chan_sender == ERR_PTR(-ENODEV) || !chan_sender) {
		pr_info("%s Unable to register TX nios channel", __func__);
		return -ENODEV;
	}

	chan_receiver = mbox_request_channel_byname(&client_receiver, "mbox-rx");
	if (chan_sender == ERR_PTR(-ENODEV) || !chan_sender) {
		pr_info("%s Unable to register RX nios channel", __func__);
		return -ENODEV;
	}
	
	pr_info("End: Mailbox to NIOS Init\n");

	return 0;
}

static int mailbox_nios_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mailbox_nios_pdrv = {
	.probe = mailbox_nios_probe,
	.remove = mailbox_nios_remove,
	.driver = {
		.name  = DRIVER_NAME,
		.of_match_table = mbox_nios_of_match,
	},
};

static int communication_driver_init(void)
{
	int ret;

	pr_info("Communication Driver Init\n");

	 if (register_chrdev_region(driver_dev, driver_count, DRIVER_NAME)) {
		pr_err("Failed to load  driver\n");
		return -ENODEV;
	}
	cdev_init(&driver_cdev, &driver_fops);

	if (cdev_add(&driver_cdev, driver_dev, driver_count)) {
		unregister_chrdev_region(driver_dev, driver_count);
		pr_err("Failed to load  driver\n");
		return -ENODEV;
	}

	if ((cl = class_create(THIS_MODULE, "communicationdrv")) == NULL) {
		pr_err("class_create failed\n");
	} else if (device_create(cl, NULL, driver_dev, NULL, DRIVER_NAME) == NULL)	{
		pr_err("device_create failed\n");
	}

	ret = platform_driver_probe(&mailbox_nios_pdrv, mailbox_nios_probe);
	if (ret) {
		pr_err("%s Unable to register driver\n", __func__);
		return ret;
	}

	return 0;
}

/**
 * @brief ioctl communicaton function
 *        communicaton with application
 * @param data passed from application memory space
 *
 * @return communicaton status
 */
static long driver_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	uint32_t mess;

	switch (cmd){
	case SET_IMAGE_STATUS:
		
		if (copy_from_user(&mess, (void __user *)arg, sizeof(unsigned int)))
			return COMMUNICATION_ERROR;

		/* Send message to nios */
		ret = mbox_send_message(chan_sender, (void *)&mess);
		pr_info("%s image is sent to nios, size: %d", __func__, mess);

		if (ret < 0)
			pr_info("Message 1 failure\n");

		set_process_status(PROCESS_START);
		if(__put_user(ret, (unsigned int __user *)arg))
				return COMMUNICATION_ERROR;
		break;

	case GET_IMAGE_STATUS:
		mess = get_process_status();
		/*
		 * Return image status
		 * If process_status is 0,
		 * Nios did not finish to process the last request
		 */
		if(__put_user(mess, (unsigned int __user *)arg))
			return COMMUNICATION_ERROR;
		break;
	default:
		break;
	}

	return ret;
}

static struct file_operations driver_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = driver_ioctl,
};

static void communication_driver_exit(void)
{
	pr_info("Communication Driver exit\n");

	cdev_del(&driver_cdev);
	unregister_chrdev_region(driver_dev, driver_count);
	device_destroy(cl, driver_dev);
	class_destroy(cl);
	mbox_free_channel(chan_sender);
	mbox_free_channel(chan_receiver);
	platform_driver_unregister(&mailbox_nios_pdrv);
}

module_init(communication_driver_init);
module_exit(communication_driver_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NIOS II communication channels");
MODULE_AUTHOR("Anes Hadziahmetagic <anes.hadziahmetagic@gmail.com>");

