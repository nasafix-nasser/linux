/*
 * =====================================================================================
 *
 *       Filename:  led.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  11/30/2010 07:42:29 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Gavin.S (), zdshuwei@gmail.com
 *        Company:  Zhejiang University
 *
 * =====================================================================================
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h> 
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/delay.h>

#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/uaccess.h>
#include <linux/pci.h>

#include <linux/proc_fs.h>

#define DEVICE_NAME		"ledtest"
#define LED_MAJOR		259

#define ON		0
#define OFF		1

/*  注册设备 */
struct leds_dev
{
	struct cdev cdev;
	unsigned char value;
};

/*  指定LED引脚 */
static unsigned long led_table [] =
{
	S5PV210_GPH0(6),
};

/*  GPIO输出功能 */
static unsigned int led_cfg_table [] =
{
	1,
};

static int leds_open(struct inode *inode, struct file *file)
{
	int i;

	s3c_gpio_cfgpin(S5PV210_GPH0(6), 1);

	return 0;
}

static int leds_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{

	switch(cmd)
	{
		case ON:
			gpio_direction_output(S5PV210_GPH0(6), 1);
			printk("pull up \n");
			return 0;
		case OFF:
			gpio_direction_output(S5PV210_GPH0(6), 0);
			printk("pull down \n");
			return 0;
			
		default:
			return -EINVAL;
	}
}

static struct file_operations s3c6410_leds_fops = 
{
	.owner	=	THIS_MODULE,
	.open	=	leds_open,
	.ioctl	=	leds_ioctl,
};

static void leds_setup_cdev(struct leds_dev *dev)
{
	int err;
	int devno = MKDEV(LED_MAJOR, 0);

	cdev_init(&dev->cdev, &s3c6410_leds_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &s3c6410_leds_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding LED", err);
}

static int __init leds_init(void)
{
	int ret;

	ret = register_chrdev(LED_MAJOR, DEVICE_NAME, &s3c6410_leds_fops);
	if (ret < 0)
	{
		printk(DEVICE_NAME "can't register major number.\n");
		return ret;
	}
	

	struct class *my_class = class_create(THIS_MODULE, "my_device_driver");
	device_create(my_class, NULL, MKDEV(LED_MAJOR, 0), NULL, "ledtest");

	printk(DEVICE_NAME "initialized.\n");
	return 0;
}

static void __exit leds_exit(void)
{
	unregister_chrdev(LED_MAJOR, DEVICE_NAME);
}


module_init(leds_init);
module_exit(leds_exit);

MODULE_AUTHOR("Gavin.S");
MODULE_DESCRIPTION("s3c6410 led driver test.");
MODULE_LICENSE("GPL");
