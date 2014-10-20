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

#define DEVICE_NAME "real_led"

static int led_gpios[] = {
	S5PV210_GPH0(6),
};

#define LED_NUM		ARRAY_SIZE(led_gpios)

/*主设备和从设备号变量*/  
static int led_major = 0;  
static int led_minor = 0;  

static struct class* led_class = NULL; 

/*访问设置属性方法*/  
static ssize_t led_val_show(struct device* dev, struct device_attribute* attr,  char* buf);  
static ssize_t led_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);  
  
/*定义设备属性*/  
static DEVICE_ATTR(val, S_IRUGO | S_IWUSR, led_val_show, led_val_store);  

struct leds_dev
{
struct cdev dev;
int led_status;
};

struct leds_dev *led_dev=NULL;






/*打开设备方法*/  
static int led_open(struct inode* inode, struct file* filp) {  
    struct leds_dev* dev;          
      
    /*将自定义设备结构体保存在文件指针的私有数据域中，以便访问设备时拿来用*/  
    dev = container_of(inode->i_cdev, struct leds_dev, dev);  
    filp->private_data = dev;  
      
    return 0;  
}  
  
/*设备文件释放时调用，空实现*/  
static int led_release(struct inode* inode, struct file* filp) {  
    return 0;  
}  
  
/*读取设备的寄存器val的值*/  
static ssize_t led_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos) {  
    ssize_t err = 0;  
    struct leds_dev* dev = filp->private_data;          
  
  
  
    if(count < sizeof(dev->led_status)) {  
        goto out;  
    }          
  
    /*将寄存器val的值拷贝到用户提供的缓冲区*/  
    if(copy_to_user(buf, &(dev->led_status), sizeof(dev->led_status))) {  
        err = -EFAULT;  
        goto out;  
    }  
  
    err = sizeof(dev->led_status);  
  
out:  
    return err;  
}  
  
/*写设备的寄存器值val*/  
static ssize_t led_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos) {  
    struct leds_dev* dev = filp->private_data;  
    ssize_t err = 0;          
  	int i;
    
  
    if(count != sizeof(dev->led_status)) {  
        goto out;          
    }          
  
    /*将用户提供的缓冲区的值写到设备寄存器去*/  
    if(copy_from_user(&(dev->led_status), buf, count)) {  
        err = -EFAULT;  
        goto out;  
    }  
  
	for(i=0;i<LED_NUM;i++){
		if((0x01&(dev->led_status>>i))==1)
			gpio_set_value(led_gpios[i], 0);
		else
			gpio_set_value(led_gpios[i], 1);

	}

    err = sizeof(dev->led_status);  
  
out:   
    return err;  
}  

/**************************************************************************************/

/*设备文件操作方法表*/  
static struct file_operations led_fops = {  
    .owner = THIS_MODULE,  
    .open = led_open,  
    .release = led_release,  
    .read = led_read,  
    .write = led_write,   
};  

/**************************************************************************************/

static ssize_t __led_get_val(struct leds_dev* dev, char* buf) {  
    int val = 0;          
  
       
  
    val = dev->led_status;                  
  
    return snprintf(buf, PAGE_SIZE, "%d\n", val);  
}  
  
/*把缓冲区buf的值写到设备寄存器val中去，内部使用*/  
static ssize_t __led_set_val(struct leds_dev* dev, const char* buf, size_t count) {  
    int val = 0;          
    int i;
    /*将字符串转换成数字*/          
    val = simple_strtol(buf, NULL, 10);          
	for(i=0;i<LED_NUM;i++){
		if((0x01&(val>>i))==1)
			gpio_set_value(led_gpios[i], 0);
		else
			gpio_set_value(led_gpios[i], 1);

	}
      
  
    dev->led_status = val;          
  
  
    return count;  
}  
  
/*读取设备属性val*/  
static ssize_t led_val_show(struct device* dev, struct device_attribute* attr, char* buf) {  
    struct leds_dev* hdev = (struct leds_dev*)dev_get_drvdata(dev);          
  
    return __led_get_val(hdev, buf);  
}  
  
/*写设备属性val*/  
static ssize_t led_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) {   
    struct leds_dev* hdev = (struct leds_dev*)dev_get_drvdata(dev);    
      
    return __led_set_val(hdev, buf, count);  
}  



/**************************************************************************************/

/*读取设备寄存器val的值，保存在page缓冲区中*/  
static ssize_t led_proc_read(char* page, char** start, off_t off, int count, int* eof, void* data) {  
    if(off > 0) {  
        *eof = 1;  
        return 0;  
    }  
  
    return __led_get_val(led_dev, page);  
}  
  
/*把缓冲区的值buff保存到设备寄存器val中去*/  
static ssize_t led_proc_write(struct file* filp, const char __user *buff, unsigned long len, void* data) {  
    int err = 0;  
    char* page = NULL;  
  
    if(len > PAGE_SIZE) {  
        printk(KERN_ALERT"The buff is too large: %lu./n", len);  
        return -EFAULT;  
    }  
  
    page = (char*)__get_free_page(GFP_KERNEL);  
    if(!page) {                  
        printk(KERN_ALERT"Failed to alloc page./n");  
        return -ENOMEM;  
    }          
  
    /*先把用户提供的缓冲区值拷贝到内核缓冲区中去*/  
    if(copy_from_user(page, buff, len)) {  
        printk(KERN_ALERT"Failed to copy buff from user./n");                  
        err = -EFAULT;  
        goto out;  
    }  
  
    err = __led_set_val(led_dev, page, len);  
  
out:  
    free_page((unsigned long)page);  
    return err;  
}  
  
/*创建/proc/led文件*/  
static void led_create_proc(void) {  
    struct proc_dir_entry* entry;  
      
    entry = create_proc_entry(DEVICE_NAME, 0, NULL);  
    if(entry) {  
    //    entry->owner = THIS_MODULE;  
        entry->read_proc = led_proc_read;  
        entry->write_proc = led_proc_write;  
    }  
}  
  
/*删除/proc/led文件*/  
static void led_remove_proc(void) {  
    remove_proc_entry(DEVICE_NAME, NULL);  
}  

/**************************************************************************************/




static int  __led_setup_dev(struct leds_dev* dev) {  
    int err;  
    dev_t devno = MKDEV(led_major, led_minor);  
  
    memset(dev, 0, sizeof(struct leds_dev));  
  
    cdev_init(&(led_dev->dev), &led_fops);  
    dev->dev.owner = THIS_MODULE;  
    dev->dev.ops = &led_fops;          
  
    /*注册字符设备*/  
    err = cdev_add(&(dev->dev),devno, 1);  
    if(err) {  
        return err;  
    }          
  
    return 0;  
}  





static int __init real210_led_dev_init(void) {
	int ret;
	int i;
	unsigned int val;
	int err = -1;  
    	dev_t dev = 0;  
	 struct device* temp = NULL; 
    


	for (i = 0; i < LED_NUM; i++) {
		ret = gpio_request(led_gpios[i], "LED");
		if (ret) {
			printk("%s: request GPIO %d for LED failed, ret = %d\n", DEVICE_NAME,
					led_gpios[i], ret);
			goto fail;
		}

		s3c_gpio_cfgpin(led_gpios[i], S3C_GPIO_OUTPUT);
		gpio_set_value(led_gpios[i], 1);
	}

  
    /*动态分配主设备和从设备号*/  
    err = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);  
    if(err < 0) {  
        printk(KERN_ALERT"Failed to alloc char dev region./n");  
        goto fail;  
    }  
  
    led_major = MAJOR(dev);  
    led_minor = MINOR(dev);          
  
    /*分配led设备结构体变量*/  
    led_dev = kmalloc(sizeof(struct leds_dev), GFP_KERNEL);  
    if(!led_dev) {  
        err = -ENOMEM;  
        printk(KERN_ALERT"Failed to alloc led_dev./n");  
        goto unregister;  
    }          
  
    /*初始化设备*/  
    err = __led_setup_dev(led_dev);  
    if(err) {  
        printk(KERN_ALERT"Failed to setup dev: %d./n", err);  
        goto cleanup;  
    }          





    /*在/sys/class/目录下创建设备类别目录led*/  
    led_class = class_create(THIS_MODULE, DEVICE_NAME);  
    if(IS_ERR(led_class)) {  
        err = PTR_ERR(led_class);  
        printk(KERN_ALERT"Failed to create led class./n");  
        goto destroy_cdev;  
    }          
  
    /*在/dev/目录和/sys/class/led目录下分别创建设备文件led*/  
    temp = device_create(led_class, NULL, dev, "%s",DEVICE_NAME);  
    if(IS_ERR(temp)) {  
        err = PTR_ERR(temp);  
        printk(KERN_ALERT"Failed to create led device.");  
        goto destroy_class;  
    }          
  
    /*在/sys/class/led/led目录下创建属性文件val*/  
    err = device_create_file(temp, &dev_attr_val);  
    if(err < 0) {  
        printk(KERN_ALERT"Failed to create attribute val.");                  
        goto destroy_device;  
    }  
  
    dev_set_drvdata(temp, led_dev);  
	
	led_create_proc();  

    printk(DEVICE_NAME"\tinitialized\n");

    return ret;


	
	destroy_device:  
	    device_destroy(led_class, dev);  
	  
	destroy_class:  
	    class_destroy(led_class);  
	  
	destroy_cdev:  
	    cdev_del(&(led_dev->dev));  
	  
	cleanup:  
	    kfree(led_dev);  
	  
	unregister:  
	    unregister_chrdev_region(MKDEV(led_major, led_minor), 1);  
	  
	fail:  
	for (; i >=0; i--)
		gpio_free(led_gpios[i]);

	return err; 



}

static void __exit real210_led_dev_exit(void) {
	
	int i;
	dev_t devno = MKDEV(led_major, led_minor);  

	for (i = 0; i < LED_NUM; i++) {
		gpio_free(led_gpios[i]);
	}


  
            
  
    /*删除/proc/led文件*/  
   led_remove_proc();          
  
    /*销毁设备类别和设备*/  
    if(led_class) {  
        device_destroy(led_class, MKDEV(led_major, led_minor));  
        class_destroy(led_class);  
    }          
  
    /*删除字符设备和释放设备内存*/  
    if(led_dev) {  
        cdev_del(&(led_dev->dev));  
        kfree(led_dev);  
    }          
  
    /*释放设备号*/  
    unregister_chrdev_region(devno, 1);  

	printk(KERN_ALERT"Destroy led device./n");  
}

module_init(real210_led_dev_init);
module_exit(real210_led_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kuangrenyu <hzzhangguofeng@gmail.com>");

