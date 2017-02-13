
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <linux/cdev.h>

#include "adxl345.h"

static struct sensor_adxl345{
	struct spi_device *adxl345_spi;
	struct mutex lock;
	u16 axis_data[AXIS];
	//struct spi_transfer adxl345_transfer;
	struct cdev c_dev;
	dev_t adxl345_dev_number;
	struct class *adxl345_class;
};

static struct sensor_adxl345 *adxl345;

static int adxl345_readings(void)
{
	unsigned char buf;
	int err,
	    i;		/*iterator*/
	buf = DATA_START;
	mutex_lock(&adxl345->lock);
	err = spi_write_then_read(adxl345->adxl345_spi, &buf, 1, (u8 *) adxl345->axis_data, 6);
	mutex_unlock(&adxl345->lock);
	if(err){
		printk(KERN_DEBUG "ADXL345: Cannot read.\n");
		return err;
	}
	for(i = 0; i < AXIS; i++){
		mutex_lock(&adxl345->lock);
		adxl345->axis_data[i] = be16_to_cpu(adxl345->axis_data[i]);
		mutex_unlock(&adxl345->lock);
	}
	return 0;
}
	
static int adxl345_read_reg(struct spi_device* spi, unsigned char address, void* data)
{
	unsigned char buf[2];
	buf[0] = address;
	return spi_write_then_read(spi, buf, 1, buf, 1);
}

static int adxl345_write_reg(struct spi_device* spi, unsigned char address, unsigned char data)
{
	unsigned char buf[2];
	buf[0] = address;
	return spi_write_then_read(spi, buf, 2, NULL, 0);
}

static int data_format_config(struct spi_device* spi)
{
	u8 data_format;
	int err;
	data_format = 0x01;
	err = adxl345_write_reg(spi, DATA_FORMAT, data_format);
	if(err){
		printk(KERN_DEBUG "ADXL345: Cannot configure data format.\n");
		return err;
	}
	return 0;
}

static int power_configure(struct spi_device* spi)
{
	u8 power_ctl;
	int err;
	power_ctl = 0x08;
	err = adxl345_write_reg(spi, POWER_CTL, power_ctl);
	if(err){
		printk(KERN_DEBUG "ADXL345: Power Reg can't be configured.\n");
		return err;
	}
	return 0;
}

static int fifo_control(struct spi_device* spi)
{
	u8 fifo_ctl;
	int err;
	fifo_ctl = 0x00;
	err = adxl345_write_reg(spi, FIFO_CTL, fifo_ctl);
	if(err){
		printk(KERN_DEBUG "ADXL345: FIFO Control Register can't be configured.\n");
		return err;
	}
	return 0;
}

static int adxl345_probe(struct spi_device *spi)
{
	int err;
	mutex_lock(&adxl345->lock);
	adxl345->adxl345_spi = spi;
	mutex_unlock(&adxl345->lock);
	err = adxl345_readings();
	if(err){
		printk(KERN_DEBUG "ADXL345: Cannot get any readings\n");
		return err;
		}
	mutex_lock(&adxl345->lock);
	printk(KERN_DEBUG "ADXL345: %hd %hd %hd \n", adxl345->axis_data[0], adxl345->axis_data[1], adxl345->axis_data[2]);
	mutex_unlock(&adxl345->lock);
	mutex_lock(&adxl345->lock);
	data_format_config(adxl345->adxl345_spi);
	power_configure(adxl345->adxl345_spi);
	fifo_control(adxl345->adxl345_spi);
	mutex_unlock(&adxl345->lock);
	/*adxl345->adxl345_spi->max_speeed_hz = 5000000;
	err = spi_setup(adxl345->adxl345_spi);
	if(!err){
		printk(KERN_DEBUG "ADXL345: Cannot setup the device\n");
		return err;
	}*/
	//spi_cmd(SPI1, ENABLE);
	printk(KERN_DEBUG "ADXL345: Probe completed\n");
	return 0;
}

static int adxl345_remove(struct spi_device *spi)
{
	return 0;
	}

static const struct of_device_id adxl345_of_match[] = {
	{
		.compatible = "adxl345",
		.data = 0,
	},
	{}
};
MODULE_DEVICE_TABLE(of, adxl345_of_match);

static struct spi_driver adxl345_driver = {
	.driver = {
		.name = SENSOR_ID,
		.owner = THIS_MODULE,
		.of_match_table = adxl345_of_match,
	},
	.probe = adxl345_probe,
	.remove = adxl345_remove,
};

static int adxl345_open(struct inode *inode, struct file *file)
{
	printk(KERN_DEBUG "ADXL345: Open called\n");
	return 0;
}

static int adxl345_release(struct inode *inode, struct file *file)
{
	printk(KERN_DEBUG "ADXL345: Release called\n");
	return 0;
}

static long adxl345_ioctl(struct file *fi, unsigned int cmd, unsigned long arg)
{
	mutex_lock(&adxl345->lock);
	struct spi_device* spi = adxl345->adxl345_spi;
	mutex_unlock(&adxl345->lock);
	switch(cmd){
		case ADXL345_READ:
			adxl345_readings();
			mutex_lock(&adxl345->lock);
			if(copy_to_user((unsigned short *)arg, adxl345->axis_data, 6)){
				mutex_unlock(&adxl345->lock);
				return -EFAULT;
			}
			mutex_unlock(&adxl345->lock);
			return 0;
		default:
			return -ENOTTY;
	}
}

static const struct file_operations adxl345_fops = {
	.owner = THIS_MODULE,
	.open = adxl345_open,
	.release = adxl345_release,
	.unlocked_ioctl = adxl345_ioctl,
};

static int __init adxl345_init(void)
{
	int error;

	adxl345 = kzalloc(sizeof(struct sensor_adxl345), GFP_KERNEL);
	if(!adxl345){
		printk(KERN_DEBUG "ADXL345: Cannot create adxl345 structure\n");
		return -ENOMEM;
	}
	
	if(alloc_chrdev_region(&adxl345->adxl345_dev_number, 0, 1, "adxl345")){
		printk(KERN_DEBUG "ADXL345: Cannot register char device\n");
		return -1;
	}
	
	adxl345->adxl345_class = class_create(THIS_MODULE, "adxl345-spi");
	cdev_init(&adxl345->c_dev, &adxl345_fops);
	error = cdev_add(&adxl345->c_dev, adxl345->adxl345_dev_number, 1);
	if(error<0){
		printk(KERN_DEBUG "ADXL345: Cannot add device\n");
		unregister_chrdev_region(adxl345->adxl345_dev_number, 1);
		return -1;
	}
	mutex_init(&adxl345->lock);
	spi_register_driver(&adxl345_driver);
	device_create(adxl345->adxl345_class, NULL, adxl345->adxl345_dev_number,NULL, "adxl345");
	printk(KERN_DEBUG "ADXL345: Major number: %d Minor number: %d\n", MAJOR(adxl345->adxl345_dev_number), MINOR(adxl345->adxl345_dev_number));
	return 0;
}

static void __exit adxl345_exit(void)
{
	spi_unregister_driver(&adxl345_driver);

	device_destroy(adxl345->adxl345_class, adxl345->adxl345_dev_number);
	class_destroy(adxl345->adxl345_class);

	cdev_del(&adxl345->c_dev);
	unregister_chrdev_region(adxl345->adxl345_dev_number, 1);

}

module_init(adxl345_init);
module_exit(adxl345_exit);
MODULE_LICENSE("GPL v2");
