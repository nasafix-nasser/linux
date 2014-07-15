/*
 * drivers/video/samsung/ch7034.c
 *
 * $Id: ch7034.c,v 1.12 2009/09/13 02:13:24 
 *
 * Copyright (C) 2009 Figo Wang <sagres_2004@163.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	S3C Frame Buffer Driver
 *	based on skeletonfb.c, sa1100fb.h, s3c2410fb.c
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "s3cfb.h"
#include "ch7034.h"

#define CH7034_I2C_ADDR		0x75		//AS下拉:0x76 上拉:0x75
#define I2C_BUS_NUM			0


static void ch7034_i2c_write(struct i2c_client *client,unsigned char addr, unsigned char data)
{
	unsigned char buffer[2];
	
	buffer[0] = addr;
	buffer[1] = data;	
	
	i2c_master_send(client, buffer, 2);
}


static unsigned char ch7034_i2c_read(struct i2c_client *client,unsigned char addr)
{
	unsigned char buffer[1];
	int rc;
	buffer[0] = addr;
	if (1 != (rc = i2c_master_send(client, buffer, 1)))
		printk("i2c i/o error: rc == %d (should be 1)\n", rc);

	msleep(10);

	if (1 != (rc = i2c_master_recv(client, buffer, 1)))
	{
		printk("i2c i/o error: rc == %d (should be 1)\n", rc);	
	   return -ENODEV;
	}
	else
	{
		return buffer[0];
	}
}


static int ch7034_start(struct i2c_client *client)
{
	u32 i;
	u32 val_t;
	u32 hinc_reg, hinca_reg, hincb_reg;
	u32 vinc_reg, vinca_reg, vincb_reg;
	u32 hdinc_reg, hdinca_reg, hdincb_reg;

	//1. write register table:
	for(i=0; i<REGTABLE_LEN; ++i)
	{
		i2c_master_send(client, CH7034_RegTable[i],sizeof(CH7034_RegTable[i]));
	}
	
	//2. Calculate online parameters:
	ch7034_i2c_write(client, 0x03, 0x00);
	i = ch7034_i2c_read(client, 0x25);
	ch7034_i2c_write(client, 0x03, 0x04);
	//HINCA:
	val_t = ch7034_i2c_read(client, 0x2A);
	hinca_reg = (val_t << 3) | (ch7034_i2c_read(client, 0x2B) & 0x07);
	//HINCB:
	val_t = ch7034_i2c_read(client, 0x2C);
	hincb_reg = (val_t << 3) | (ch7034_i2c_read(client, 0x2D) & 0x07);
	//VINCA:
	val_t = ch7034_i2c_read(client, 0x2E);
	vinca_reg = (val_t << 3) | (ch7034_i2c_read(client, 0x2F) & 0x07);
	//VINCB:
	val_t = ch7034_i2c_read(client, 0x30);
	vincb_reg = (val_t << 3) | (ch7034_i2c_read(client, 0x31) & 0x07);
	//HDINCA:
	val_t = ch7034_i2c_read(client, 0x32);
	hdinca_reg = (val_t << 3) | (ch7034_i2c_read(client, 0x33) & 0x07);
	//HDINCB:
	val_t = ch7034_i2c_read(client, 0x34);
	hdincb_reg = (val_t << 3) | (ch7034_i2c_read(client, 0x35) & 0x07);
	//no calculate hdinc if down sample disaled
	if(i & (1 << 6))
	{
		if(hdincb_reg == 0)
		{
			return -ENODEV;
		}

		hdinc_reg = (u32)((hdinca_reg) * (1 << 20) / hdincb_reg);
		ch7034_i2c_write(client, 0x3C, (hdinc_reg >> 16) & 0xFF);
		ch7034_i2c_write(client, 0x3D, (hdinc_reg >>  8) & 0xFF);
		ch7034_i2c_write(client, 0x3E, (hdinc_reg >>  0) & 0xFF);
	}
	if(hincb_reg == 0 || vincb_reg == 0)
	{
		return -ENODEV;
	}
	if(hinca_reg > hincb_reg)
	{
		return -ENODEV;
	}

	hinc_reg = (u32)(hinca_reg * (1 << 20) / hincb_reg);

	vinc_reg = (u32)(vinca_reg * (1 << 20) / vincb_reg);
	ch7034_i2c_write(client, 0x36, (hinc_reg >> 16) & 0xFF);
	ch7034_i2c_write(client, 0x37, (hinc_reg >>  8) & 0xFF);
	ch7034_i2c_write(client, 0x38, (hinc_reg >>  0) & 0xFF);
	ch7034_i2c_write(client, 0x39, (vinc_reg >> 16) & 0xFF);
	ch7034_i2c_write(client, 0x3A, (vinc_reg >>  8) & 0xFF);
	ch7034_i2c_write(client, 0x3B, (vinc_reg >>  0) & 0xFF);

	//3. Start to running:
	ch7034_i2c_write(client, 0x03, 0x00);
	val_t = ch7034_i2c_read(client, 0x0A);
	ch7034_i2c_write(client, 0x0A, val_t | 0x80);
	ch7034_i2c_write(client, 0x0A, val_t & 0x7F);
	val_t = ch7034_i2c_read(client, 0x0A);
	ch7034_i2c_write(client, 0x0A, val_t & 0xEF);
	ch7034_i2c_write(client, 0x0A, val_t | 0x10);
	ch7034_i2c_write(client, 0x0A, val_t & 0xEF);
	printk("ch7034 init ok\n");
	return 0;
}


static int __devinit ch7034_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	dev_info(&client->adapter->dev, "ch7034 probe\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
	{
		printk("i2c_check_functionality error\n");
//		i2c_unregister_device(client);
		return -ENODEV;
	}
	
	if(!ch7034_start(client))
		printk("ch7034 probe successfully\n");

//	i2c_set_clientdata(client, &ch7034_data);

	return 0;
}

static int ch7034_remove(struct i2c_client *client)
{
//	i2c_unregister_device(client);
	i2c_set_clientdata(client, NULL);
	
//	if (client == ch7034_data.client)
//		ch7034_data.client = NULL;

	return 0;
}

static const struct i2c_device_id ch7034_i2c_id[] = {
	{ "ch7034", 0 }
};

static struct i2c_driver ch7034_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ch7034",
	},

//	.attach_adapter = ch7034_attach_adapter,
	.probe = ch7034_probe,
	.remove = __devexit_p(ch7034_remove),
	.id_table = ch7034_i2c_id,
};

static int ch7034_attach_adapter(void)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	printk("[VGA] ch7034_attach_adapter.\n");
	
	adapter = i2c_get_adapter(I2C_BUS_NUM);

	memset(&info, 0, sizeof(struct i2c_board_info));

	strlcpy(info.type, "ch7034", I2C_NAME_SIZE);
	info.addr = CH7034_I2C_ADDR;
	client = i2c_new_device(adapter, &info);
	if (!client)
	{
		printk("failed to add i2c driver\n");
		return -ENODEV;
	}
	return 0;
}


static int __init ch7034_init(void)
{
	int ret;
	ch7034_attach_adapter();
	ret = i2c_add_driver(&ch7034_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register ch7034 I2C driver: %d\n", ret);

	return ret;
}

static void __exit ch7034_exit(void)
{
	i2c_del_driver(&ch7034_i2c_driver);
}

module_init(ch7034_init);
module_exit(ch7034_exit);

MODULE_AUTHOR("Figo Wang <sagres_2004@163.com>");
MODULE_DESCRIPTION("Texas Instruments CH7034 driver");
MODULE_LICENSE("GPL");

