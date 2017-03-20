

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include "bmp280.h"

static int bmp280_write(struct spi_device *spi, u8 address, u8 data)
{	
	u8 tx_buf[2];
	tx_buf[0] = WR_ADDRESS & address;
	tx_buf[1] = data;
	return spi_write_then_read(spi, tx_buf, 2, NULL, 0);
	}
	
static int bmp280_read(struct spi_device *spi, u8 address, u8 *data, int count)
{
	u8 tx_buf;
	tx_buf = RD_ADDRESS | address;
	return spi_write_then_read(spi, &tx_buf, 1, data, count);
	}

static ssize_t bmp280_id(struct spi_device *spi)
{
	return spi_w8r8(spi, RD_ADDRESS | ID);
}
	
static ssize_t bmp280_get_id(struct device *dev, 
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device *spi = dev_get_drvdata(dev);
	u8 id = 0;
	id = bmp280_id(spi);
	return sprintf(buf, "%u\n", id);
}

static DEVICE_ATTR(id, 0664, bmp280_get_id, NULL);

static int bmp280_probe(struct spi_device *spi)
{
	printk(KERN_DEBUG "BMP280: Probe called\n");
	u8 id;
	u8 temp[2];
	u8 pressure[2];
	spi->max_speed_hz = 5000000;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi_setup(spi);
	bmp280_write(spi, RESET, TO_RESET);
	bmp280_write(spi, CONFIG, NORMAL_CONFIG);
	bmp280_write(spi, CTRL_MEAS, NORMAL_CTRL);
	device_create_file( spi->device, &dev_attr_id);
	return 0;
	}
	
static int bmp280_remove(struct spi_device *spi)
{
	return 0;
	}

static const struct of_device_id bmp280_of_match[] = {
	{
		.compatible = "bmp280",
		.data = 0,
	},
	{}
};
MODULE_DEVICE_TABLE(of, bmp280_of_match);

static struct spi_driver bmp280_driver = {
	.driver = {
		.name = SENSOR_ID,
		.owner = THIS_MODULE,
		.of_match_table = bmp280_of_match,
	},
	.probe = bmp280_probe,
	.remove = bmp280_remove,
};

static int __init bmp280_init(void)
{
	spi_register_driver(&bmp280_driver);
	printk(KERN_DEBUG "BMP280: INIT CALLED\n");
	return 0;
	}
	
static void __exit bmp280_exit(void)
{
	spi_unregister_driver(&bmp280_driver);
	printk(KERN_DEBUG "BMP280: REMOVED\n");
}


module_init(bmp280_init);
module_exit(bmp280_exit);
MODULE_LICENSE("GPL v2");
