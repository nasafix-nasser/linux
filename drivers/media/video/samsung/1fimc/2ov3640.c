#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-i2c-drv.h>
#include <media/ov3640_platform.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/pwm_backlight.h>
#include <linux/spi/spi.h>
#include <linux/gpio_keys.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/regs-mem.h>
#include <mach/regs-gpio.h>
//#include <mach/gpio-bank.h>
#include <plat/s5pv210.h>
#include <plat/gpio-cfg.h>
//#include <mach/gpio-bank-b.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "ov3640.h"

#define OV3640_DRIVER_NAME	"OV3640"

/* Default resolution & pixelformat. plz ref ov3640_platform.h */
#define DEFAULT_RES		WVGA	/* Index of resoultion */
#define DEFAUT_FPS_INDEX	OV3640_15FPS
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

/* Camera functional setting values configured by user concept */
struct ov3640_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
	unsigned int glamour;
};

struct ov3640_state {
	struct ov3640_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct ov3640_userset userset;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
};

static inline struct ov3640_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov3640_state, sd);
}

/*
 * OV3640 register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
static inline int ov3640_write(struct v4l2_subdev *sd, u8 addr, u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[1];
	unsigned char reg[2];
	int err = 0;
	int retry = 0;


	if (!client->adapter)
		return -ENODEV;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = reg;

	reg[0] = addr & 0xff;
	reg[1] = val & 0xff;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return err;	/* Returns here on success */

	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%02x%02x, " \
			"value: 0x%02x%02x\n", __func__, \
			reg[0], reg[1], reg[2], reg[3]);
		retry++;
		goto again;
	}

	return err;
}

static int ov3640_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[],
				unsigned char length)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
//	unsigned char buf[length], i;
//	struct i2c_msg msg = {client->addr, 0, length, buf};

//	for (i = 0; i < length; i++)
//		buf[i] = i2c_data[i];

//	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;

	return i2c_master_send(client, i2c_data, length);
}

static int ov3640_write_regs(struct v4l2_subdev *sd, unsigned char regs[],
				int size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, err;
	
	for (i = 0; i < size; i++) {
		err = ov3640_i2c_write(sd, &regs[i], sizeof(regs[i]));
		if (err < 0)
			v4l_info(client, "%s: register set failed\n", \
			__func__);
	}

	return 0;	/* FIXME */
}

static const char *ov3640_querymenu_wb_preset[] = {
	"WB Tungsten", "WB Fluorescent", "WB sunny", "WB cloudy", NULL
};

static const char *ov3640_querymenu_effect_mode[] = {
	"Effect Sepia", "Effect Aqua", "Effect Monochrome",
	"Effect Negative", "Effect Sketch", NULL
};

static const char *ov3640_querymenu_ev_bias_mode[] = {
	"-3EV",	"-2,1/2EV", "-2EV", "-1,1/2EV",
	"-1EV", "-1/2EV", "0", "1/2EV",
	"1EV", "1,1/2EV", "2EV", "2,1/2EV",
	"3EV", NULL
};

static struct v4l2_queryctrl ov3640_controls[] = {
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 0,
		.maximum = 10000,
		.step = 1,
		.default_value = 5000,//0,	/* FIXME */
	},
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov3640_querymenu_wb_preset) - 2,
		.step = 1,
		.default_value = ARRAY_SIZE(ov3640_querymenu_wb_preset)/2,//0,
	},
	{
		.id = V4L2_CID_AUTO_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto white balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 1,//0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Exposure bias",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov3640_querymenu_ev_bias_mode) - 2,
		.step = 1,
		.default_value = \
			0,//(ARRAY_SIZE(ov3640_querymenu_ev_bias_mode) - 2) / 2,
			/* 0 EV */
	},
	{
		.id = V4L2_CID_COLORFX,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Image Effect",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov3640_querymenu_effect_mode) - 2,
		.step = 1,
		.default_value = ARRAY_SIZE(ov3640_querymenu_effect_mode)/2,//0,
	},
	{
		.id = V4L2_CID_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 0,//2,
	},
	{
		.id = V4L2_CID_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 0,//2,
	},
	{
		.id = V4L2_CID_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 0,//2,
	},
};

const char **ov3640_ctrl_get_menu(u32 id)
{
		printk("%s\n", __func__);
//	printk("c->id %x\n",c->id);
	switch (id) {
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return ov3640_querymenu_wb_preset;

	case V4L2_CID_COLORFX:
		return ov3640_querymenu_effect_mode;

	case V4L2_CID_EXPOSURE:
		return ov3640_querymenu_ev_bias_mode;

	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *ov3640_find_qctrl(int id)
{
	int i;

//			printk("%s\n", __func__);
	printk("id %x\n",id);
	for (i = 0; i < ARRAY_SIZE(ov3640_controls); i++)
		if (ov3640_controls[i].id == id)
			return &ov3640_controls[i];

	return NULL;
}

static int ov3640_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;
	
	printk("%s\n", __func__);
	printk("id %x\n",qc->id);

	for (i = 0; i < ARRAY_SIZE(ov3640_controls); i++) {
		if (ov3640_controls[i].id == qc->id) {
			memcpy(qc, &ov3640_controls[i], \
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int ov3640_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;
	
	printk("%s\n", __func__);

	qctrl.id = qm->id;
	ov3640_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, ov3640_ctrl_get_menu(qm->id));
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int ov3640_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int ov3640_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}

static int ov3640_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}
static int ov3640_enum_framesizes(struct v4l2_subdev *sd, \
					struct v4l2_frmsizeenum *fsize)
{
	int err = 0;

	return err;
}

static int ov3640_enum_frameintervals(struct v4l2_subdev *sd,
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int ov3640_enum_fmt(struct v4l2_subdev *sd, struct v4l2_fmtdesc *fmtdesc)
{
	int err = 0;

	return err;
}

static int ov3640_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}

static int ov3640_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s\n", __func__);

	return err;
}

static int ov3640_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s: numerator %d, denominator: %d\n", \
		__func__, param->parm.capture.timeperframe.numerator, \
		param->parm.capture.timeperframe.denominator);

	return err;
}

static int ov3640_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov3640_state *state = to_state(sd);
	struct ov3640_userset userset = state->userset;
	int err = -EINVAL;
	printk(".....................................ov3640_g_ctrl\n");
	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		ctrl->value = userset.exposure_bias;
		err = 0;
		break;

	case V4L2_CID_AUTO_WHITE_BALANCE:
		ctrl->value = userset.auto_wb;
		printk("..................balance.\n");
		err = 0;
		break;

	case V4L2_CID_WHITE_BALANCE_PRESET:
		ctrl->value = userset.manual_wb;
		printk("..................preset.\n");
		err = 0;
		break;

	case V4L2_CID_COLORFX:
		ctrl->value = userset.effect;
		err = 0;
		break;

	case V4L2_CID_CONTRAST:
		ctrl->value = userset.contrast;
		err = 0;
		break;

	case V4L2_CID_SATURATION:
		ctrl->value = userset.saturation;
		err = 0;
		break;

	case V4L2_CID_SHARPNESS:
		ctrl->value = userset.saturation;
		err = 0;
		break;

	default:
		dev_err(&client->dev, "%s: no such ctrl\n", __func__);
		break;
	}

	return err;
}

static int ov3640_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	printk("%s\n", __func__);
#ifdef OV3640_COMPLETE
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "%s: V4L2_CID_EXPOSURE\n", __func__);
		err = ov3640_write_regs(sd, \
		(unsigned char *) ov3640_regs_ev_bias[ctrl->value], \
			sizeof(ov3640_regs_ev_bias[ctrl->value]));
		break;

	case V4L2_CID_AUTO_WHITE_BALANCE:
	    printk(".........AUTO.........balance.\n");
		dev_dbg(&client->dev, "%s: V4L2_CID_AUTO_WHITE_BALANCE\n", \
			__func__);
		err = ov3640_write_regs(sd, \
		(unsigned char *) ov3640_regs_awb_enable[ctrl->value], \
			sizeof(ov3640_regs_awb_enable[ctrl->value]));
		break;

	case V4L2_CID_WHITE_BALANCE_PRESET:
		dev_dbg(&client->dev, "%s: V4L2_CID_WHITE_BALANCE_PRESET\n", \
			__func__);
		err = ov3640_write_regs(sd, \
			(unsigned char *) ov3640_regs_wb_preset[ctrl->value], \
			sizeof(ov3640_regs_wb_preset[ctrl->value]));
		break;

	case V4L2_CID_COLORFX:
		dev_dbg(&client->dev, "%s: V4L2_CID_COLORFX\n", __func__);
		err = ov3640_write_regs(sd, \
		(unsigned char *) ov3640_regs_color_effect[ctrl->value], \
			sizeof(ov3640_regs_color_effect[ctrl->value]));
		break;

	case V4L2_CID_CONTRAST:
		dev_dbg(&client->dev, "%s: V4L2_CID_CONTRAST\n", __func__);
		err = ov3640_write_regs(sd, \
		(unsigned char *) ov3640_regs_contrast_bias[ctrl->value], \
			sizeof(ov3640_regs_contrast_bias[ctrl->value]));
		break;

	case V4L2_CID_SATURATION:
		dev_dbg(&client->dev, "%s: V4L2_CID_SATURATION\n", __func__);
		err = ov3640_write_regs(sd, \
		(unsigned char *) ov3640_regs_saturation_bias[ctrl->value], \
			sizeof(ov3640_regs_saturation_bias[ctrl->value]));
		break;

	case V4L2_CID_SHARPNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_SHARPNESS\n", __func__);
		err = ov3640_write_regs(sd, \
		(unsigned char *) ov3640_regs_sharpness_bias[ctrl->value], \
			sizeof(ov3640_regs_sharpness_bias[ctrl->value]));
		break;

	default:
		dev_err(&client->dev, "%s: no such control\n", __func__);
		break;
	}

	if (err < 0)
		goto out;
	else
		return 0;

out:
	dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed\n", __func__);
	return err;
#else
	return 0;
#endif
}

static int ov3640_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL, i;
	
	s3c_gpio_cfgpin(S5PV210_GPH2(1), S3C_GPIO_OUTPUT);
//	s3c_gpio_setpull(S5PV210_GPH2(1), S3C_GPIO_PULL_NONE);

	s3c_gpio_setpin(S5PV210_GPH2(1), 0);

printk(".....................................\n");
	v4l_info(client, "%s: camera initialization start\n", __func__);

	printk("clinet addr is 0x%x ,length is 0x%x, size is 0x%x\n",client->addr,OV3640_INIT_REGS,
									sizeof(ov3640_setting_15fps_VGA_640_480[0]));
									
	for (i = 0; i < OV3640_INIT_REGS; i++) {
		err = ov3640_i2c_write(sd, ov3640_setting_15fps_VGA_640_480[i],
					sizeof(ov3640_setting_15fps_VGA_640_480[i]));
		if (err < 0)
			v4l_info(client, "%s: register set failed\n",
			__func__);
	}
	

	if (err < 0) {
		v4l_err(client, "%s: camera initialization failed\n",
			__func__);
		return -EIO;	/* FIXME */
	}
	
	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time
 * therefor, it is not necessary to be initialized on probe time.
 * except for version checking.
 * NOTE: version checking is optional
 */
static int ov3640_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov3640_state *state = to_state(sd);
	struct ov3640_platform_data *pdata;

	dev_info(&client->dev, "fetching platform data\n");

	pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (!(pdata->default_width && pdata->default_height)) {
		/* TODO: assign driver default resolution */
	} else {
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;
	}

	if (!pdata->pixelformat)
		state->pix.pixelformat = DEFAULT_FMT;
	else
		state->pix.pixelformat = pdata->pixelformat;

	if (!pdata->freq)
		state->freq = 24000000;	/* 24MHz default */
	else
		state->freq = pdata->freq;

	if (!pdata->is_mipi) {
		state->is_mipi = 0;
		dev_info(&client->dev, "parallel mode\n");
	} else
		state->is_mipi = pdata->is_mipi;

	return 0;
}

static const struct v4l2_subdev_core_ops ov3640_core_ops = {
	.init = ov3640_init,	/* initializing API */
	.s_config = ov3640_s_config,	/* Fetch platform data */
	.queryctrl = ov3640_queryctrl,
	.querymenu = ov3640_querymenu,
	.g_ctrl = ov3640_g_ctrl,
	.s_ctrl = ov3640_s_ctrl,
};

static const struct v4l2_subdev_video_ops ov3640_video_ops = {
	.s_crystal_freq = ov3640_s_crystal_freq,
	.g_fmt = ov3640_g_fmt,
	.s_fmt = ov3640_s_fmt,
	.enum_framesizes = ov3640_enum_framesizes,
	.enum_frameintervals = ov3640_enum_frameintervals,
	.enum_fmt = ov3640_enum_fmt,
	.try_fmt = ov3640_try_fmt,
	.g_parm = ov3640_g_parm,
	.s_parm = ov3640_s_parm,
};

static const struct v4l2_subdev_ops ov3640_ops = {
	.core = &ov3640_core_ops,
	.video = &ov3640_video_ops,
};

/*
 * ov3640_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int ov3640_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct ov3640_state *state;
	struct v4l2_subdev *sd;

	dev_info(&client->adapter->dev, "ov3640 probe\n");


	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter,
	     I2C_FUNC_SMBUS_READ_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
		return -EIO;

	state = kzalloc(sizeof(struct ov3640_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, OV3640_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &ov3640_ops);
	
	// ov3640_init(sd, 0);

	dev_info(&client->dev, "ov3640 has been probed\n");
	return 0;
}


static int ov3640_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id ov3640_id[] = {
	{ OV3640_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ov3640_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = OV3640_DRIVER_NAME,
	.probe = ov3640_probe,
	.remove = ov3640_remove,
	.id_table = ov3640_id,
};

MODULE_AUTHOR("Figo Wang <sagres_2004@163.com>");
MODULE_DESCRIPTION("camera driver");
MODULE_LICENSE("GPL");
