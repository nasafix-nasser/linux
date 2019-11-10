/*************************************


*************************************/

#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/unistd.h>
#include <mach/gpio.h>

#define DEVICE_NAME "GPIO-Control"

#define IOCTL_GPIO_ON	1
#define IOCTL_GPIO_OFF	0

static unsigned long gpio_table [] =
{
	// Cortex-PC104 board
	S5PV210_GPJ3(1), //GPO#1
	S5PV210_GPJ2(7), //GPO#2
	S5PV210_GPJ3(3), //GPO#3
};



static int em210_gpio_ioctl(
	struct inode *inode, 
	struct file *file, 
	unsigned int cmd, 
	unsigned long arg)
{
	if (arg > 2)
	{
		return -EINVAL;
	}

	switch(cmd)
	{
		case IOCTL_GPIO_ON:
			gpio_set_value(gpio_table[arg], 0);
			return 0;

		case IOCTL_GPIO_OFF:
			gpio_set_value(gpio_table[arg], 1);
			return 0;

		default:
			return -EINVAL;
	}
}

static struct file_operations dev_fops = {
	.owner	=	THIS_MODULE,
	.ioctl	=	em210_gpio_ioctl,
};

static struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &dev_fops,
};

static int __init dev_init(void)
{
	int ret;

	int i;
	
	for (i = 0; i < 3; i++)
	{
		gpio_direction_output(gpio_table[i],1);
		gpio_set_value(gpio_table[i], 0);
	}

	ret = misc_register(&misc);

	return ret;
}

static void __exit dev_exit(void)
{
	misc_deregister(&misc);
}

module_init(dev_init);
module_exit(dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NASSER");
MODULE_DESCRIPTION("GPIO control for SATA Cortex-PC104 Board");
