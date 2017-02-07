/* HMC5883L Driver*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

#include "hmc5883l_ioctl.h"


#define HMC5883L_CONFIG_REG_A    0x00
    #define TOBE_CLEAR      1<<7
    #define SAMPLE_AVER     0x3
        #define SAMPLE_AVER_OFFSET   5
    #define DATA_OUT_RATE   0x7
        #define DATA_OUT_RATE_OFFSET 2
    #define MESURE_SETTING   3<<0

static int sample_average_data[] = {
    1, 2, 4, 8
};

static float data_out_rate[] = {
    0.75, 1.5, 3, 7.5, 15, 30, 75, 0
};

enum {
    NORMAL_MESURA = 0,
    POSITIVE_MESURA,
    NEGATIVE_MESURA,
    MAX_MESURA
};

#define HMC5883L_CONFIG_REG_B 0x01
    #define GAIN_SETTING 0x7
        #define GAIN_SETTING_OFFSET 5

static float gain_settings[] = {
    {0.88, 1370},
    {1.3, 1090},
    {1.9, 820},
    {2.5, 660},
    {4.0, 440},
    {4.7, 390},
    {5.6, 330},
    {8.1, 230},
};

#define HMC5883L_MODE_REG    0x02
    #define MODE_SETTING 3<<0

enum {
    CONTINOUS_MODE = 0,
    SINGLE_MODE,
    IDLE_MODE,
    MAX_MODE
};

#define HMC5883L_DATA_OUT_REG    0x03

static dev_t hmc5883l_dev_number;
static struct class *hmc5883l_class;

static struct sensor_hmc5883l{
	struct mutex lock;
	struct i2c_client *client;
	u8 sample;
	u8 out_rate;
	u8 mesura;
	u8 mode;
	u8 gain;
	u16 axis[3];
	struct cdev c_dev;
};

static struct sensor_hmc5883l *hmc5883l;


static s32 hmc5883l_write_byte(struct i2c_client *client,
        u8 reg, u8 val)
{
    return i2c_smbus_write_byte_data(client, reg, val);
}

static s32 hmc5883l_read_byte(struct i2c_client *client,
        u8 reg)
{
    return  i2c_smbus_read_byte_data(client, reg);
}

static s32 hmc5883l_read_block(void)
{
	int err, i;
	err = i2c_smbus_read_i2c_block_data(hmc5883l->client, HMC5883L_DATA_OUT_REG, 6,(u8 *)hmc5883l->axis);
	for( i = 0; i < 3; i++){
		hmc5883l->axis[i] = be16_to_cpu(hmc5883l->axis[i]);
	}
	return err;
	
}

static s32 hmc5883l_write_regA(struct i2c_client *client)
{
    u8 val;
    mutex_lock(&hmc5883l->lock);
    val = (hmc5883l->sample << SAMPLE_AVER_OFFSET)
        | (hmc5883l->out_rate << DATA_OUT_RATE_OFFSET)
        | hmc5883l->mesura;
    mutex_unlock(&hmc5883l->lock);
    return hmc5883l_write_byte(client, HMC5883L_CONFIG_REG_A, val);
}

static int hmc5883l_set_mode(struct i2c_client *client,
        u8 mode)
{
    s32 result;
    mode = mode & MODE_SETTING;
    if (mode >= MAX_MODE) {
       printk(KERN_DEBUG "HMC5883L: Invalid mode\n");
        return -EINVAL;
    }
    mutex_lock(&hmc5883l->lock);
    hmc5883l->mode = mode;
    mutex_unlock(&hmc5883l->lock);
    mode = mode | (0x0 << 7);

    result = hmc5883l_write_byte(client, HMC5883L_MODE_REG, mode);
    return (result > 0? -EBUSY : result);
}

static u8 hmc5883l_get_mode(struct i2c_client *client)
{
    return hmc5883l->mode;
}

static s32 hmc5883l_set_sample_average(struct i2c_client *client, u8 sample)
{
    sample = sample & SAMPLE_AVER;
    if (sample > 3 || sample < 0)
        return -EINVAL;
    mutex_lock(&hmc5883l->lock);
    hmc5883l->sample = sample;
    mutex_unlock(&hmc5883l->lock);
    return hmc5883l_write_regA(client);
}

static u8 hmc5883l_get_sample_average(struct i2c_client *client)
{
    return hmc5883l->sample;
}

static int hmc5883l_set_gain(struct i2c_client *client,
        u8 gain)
{
    s32 result;
    gain = gain & GAIN_SETTING;
    if (gain > 7 || gain < 0) {
        printk(KERN_DEBUG "HMC5883L: Invalid gain\n");
        return -EINVAL;
    }
    mutex_lock(&hmc5883l->lock);
    hmc5883l->gain = gain;
    mutex_unlock(&hmc5883l->lock);

    result = hmc5883l_write_byte(client, HMC5883L_CONFIG_REG_B, gain << GAIN_SETTING_OFFSET);
    return (result > 0? -EBUSY : result);
}

static u8 hmc5883l_get_gain(struct i2c_client *client)
{
    return hmc5883l->gain;
}

static s32 hmc5883l_set_mesura(struct i2c_client *client, u8 mesura)
{
    mesura = mesura & MESURE_SETTING;
    if (mesura > MAX_MESURA)
        return -EINVAL;

    mutex_lock(&hmc5883l->lock);
    hmc5883l->mesura = mesura;
    mutex_unlock(&hmc5883l->lock);
    return hmc5883l_write_regA(client);
}

static u8 hmc5883l_get_mesura(struct i2c_client *client)
{
    return hmc5883l->mesura;
}

static s32 hmc5883l_set_data_out_rate(struct i2c_client *client,
        u8 rate)
{
    rate = rate & DATA_OUT_RATE;
    if (rate >= 7 || rate < 0) {
        printk(KERN_DEBUG "HMC5883L: Invalid data out rate \n");
        return -EINVAL;
    }
    mutex_lock(&hmc5883l->lock);
    hmc5883l->out_rate = rate;
    mutex_unlock(&hmc5883l->lock);
    return hmc5883l_write_regA(client);
}

static u8 hmc5883l_get_data_out_rate(struct i2c_client *client)
{
    return hmc5883l->out_rate;
}

static long hmc5883l_ioctl(struct file *fi,
			unsigned int cmd, unsigned long arg)
			{
		printk(KERN_DEBUG "IOCTL called : %u\n", arg);
		mutex_lock(&hmc5883l->lock);
		struct i2c_client *client = hmc5883l->client;
		mutex_unlock(&hmc5883l->lock);

			switch(cmd){
				case HMC5883L_READ:
					{
					hmc5883l_read_block();
					printk(KERN_DEBUG "HMC5883L: x value - %d\n",hmc5883l->axis[0]);
					printk(KERN_DEBUG "HMC5883L: y value - %d\n",hmc5883l->axis[1]);
					printk(KERN_DEBUG "HMC5883L: z value - %d\n",hmc5883l->axis[2]);
					mutex_lock(&hmc5883l->lock);
					if(copy_to_user((unsigned short *)arg, hmc5883l->axis, 6)){
					mutex_unlock(&hmc5883l->lock);
					return -EFAULT;
					}
					mutex_unlock(&hmc5883l->lock);
					return 0;}

				case HMC5883L_GET_MODE:
					{	
						u8 mode = hmc5883l_get_mode(client);
						if(copy_to_user((unsigned short *)arg, &mode, 1)){
							       return -EFAULT;
							       }
						return 0;
						}		



				case HMC5883L_GET_SAMPLE:
				{
					u8 sample = hmc5883l_get_sample_average(client);
					if(copy_to_user((unsigned short *)arg, &sample, 1)){
							return -EFAULT;
							}
						return 0;
						}

				case HMC5883L_GET_GAIN:
				{
					u8 gain = hmc5883l_get_gain(client);
					if(copy_to_user((unsigned short *)arg, &gain, 1)){
							return -EFAULT;
							}
						return 0;
						}

				case HMC5883L_GET_MESURA:
				{
					u8 mesura = hmc5883l_get_mesura(client);
					if(copy_to_user((unsigned short *)arg, &mesura, 1)){
						return -EFAULT;
						}
					return 0;
					}

				case HMC5883L_GET_OUT_RATE:
				{
					u8 rate = hmc5883l_get_data_out_rate(client);
					if(copy_to_user((unsigned short *)arg, &rate, 1)){
							return -EFAULT;
							}
					return 0;
					}
			
				case HMC5883L_SET_MODE:
				{
					u8 mode;
					if(copy_from_user(&mode, (unsigned short *)arg, 1)){
							return -EFAULT;
						}
					if(hmc5883l_set_mode(client, mode)<0){
							return -EFAULT;
							}
							return 0;
							}

				
				case HMC5883L_SET_SAMPLE:
				{
					u8 sample;
					if(copy_from_user(&sample, (unsigned short *)arg, 1)){
							return -EFAULT;
							}
					if(hmc5883l_set_sample_average(client, sample)<0){
						return -EFAULT;
						}
						return 0;
						}

				case HMC5883L_SET_GAIN:
   				{
					u8 gain;
					if(copy_from_user(&gain, (unsigned short *)arg, 1)){
							return -EFAULT;
							}
					if(hmc5883l_set_gain(client, gain)<0){
						return -EFAULT;
						}
						return 0;
						}

	
				case HMC5883L_SET_MESURA:
				{
					u8 mesura;
					if(copy_from_user(&mesura, (unsigned short *)arg, 1)){
							return -EFAULT;
							}
					if(hmc5883l_set_mesura(client, mesura)<0){
						return -EFAULT;
						}
						return 0;
						}


				case HMC5883L_SET_OUT_RATE:
				{
					u8 rate;
					if(copy_from_user(&rate, (unsigned short *)arg, 1)){
							return -EFAULT;
							}
					if(hmc5883l_set_data_out_rate(client, rate)<0){
						return -EFAULT;
						}
						return 0;
						}


				default:
				return -ENOTTY;
			}
			}

static int hmc5883l_open(struct inode *inode, struct file *file)
{
	printk(KERN_DEBUG "HMC5883L: hmc5883l_open called\n");
	return 0;
}

static int hmc5883l_remove(struct i2c_client *client)
{
    return 0;
}

static int hmc5883l_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    int ret;
    i2c_set_clientdata(client, hmc5883l);
    hmc5883l->mesura = NORMAL_MESURA;
    hmc5883l->gain = 0x01;
    hmc5883l->mode = CONTINOUS_MODE;
    hmc5883l->out_rate = 0x04;
    hmc5883l->sample = 0x03;
    hmc5883l->client = client;
    hmc5883l_set_mode(client, hmc5883l->mode);
    hmc5883l_set_gain(client, hmc5883l->gain);
    hmc5883l_set_data_out_rate(client, hmc5883l->out_rate);
    hmc5883l_set_mesura(client, hmc5883l->mesura);
    hmc5883l_set_sample_average(client, hmc5883l->sample);
    return 0;
}

struct file_operations hmc5883l_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hmc5883l_ioctl,
	.open = hmc5883l_open,
	.read = NULL,
	.write = NULL,
	.release = NULL
};

static const struct i2c_device_id hmc5883l_id[] = {
    {"hmc5883l-i2c", 0},
    { }
};

MODULE_DEVICE_TABLE(i2c, hmc5883l_id);

static const struct of_device_id hmc5883l_of_match[] = {
{    .compatible = "honeywell,hmc5883l",
     .data = 0,
	},
     {}
};

MODULE_DEVICE_TABLE(of, hmc5883l_of_match);

static struct i2c_driver hmc5883l_driver = {
    .driver = {
        .name = "hmc5883l-i2c",
	.owner = THIS_MODULE,
	.of_match_table = hmc5883l_of_match,
    },
    .probe = hmc5883l_probe,
    .remove = hmc5883l_remove,
    .id_table = hmc5883l_id,
};

static int __init hmc5883l_init(void)
{
	int err;
	hmc5883l = kzalloc(sizeof(struct sensor_hmc5883l), GFP_KERNEL);
	if(!hmc5883l){
		printk(KERN_DEBUG "HMC5883L: Cannot create hmc5883l structure\n");
		return -ENOMEM;
	}
	if(alloc_chrdev_region(&hmc5883l_dev_number, 0, 1, "hmc5883l")){
		printk(KERN_DEBUG "HMC5883L: Can't register device\n");
		return -1;
	}
	hmc5883l_class = class_create(THIS_MODULE, "hmc5883l-i2c");
	cdev_init(&(hmc5883l->c_dev), &hmc5883l_fops);
	if(cdev_add(&(hmc5883l->c_dev),hmc5883l_dev_number, 1)){
		printk(KERN_DEBUG "HMC5883L: Can't add device");
	}
	mutex_init(&hmc5883l->lock);
	err = i2c_add_driver(&hmc5883l_driver);
	if(err){
		printk(KERN_DEBUG "HMC5883L: Registering on I2C core failed\n");
		return err; 
	}
	device_create(hmc5883l_class, NULL, hmc5883l_dev_number, NULL, "hmc5883l-i2c"); 
	printk("HMC5883L: Major number: %d Minor number: %d\n", MAJOR(hmc5883l_dev_number),MINOR(hmc5883l_dev_number));
	return 0;
}

static void __exit hmc5883l_exit(void)
{
	i2c_del_driver(&hmc5883l_driver);
	cdev_del(&(hmc5883l->c_dev));
	class_destroy(hmc5883l_class);
    if (hmc5883l->client){
        i2c_unregister_device(hmc5883l->client);
    }
    printk(KERN_DEBUG "HMC5883L: Module removed \n");
}

module_init(hmc5883l_init);
module_exit(hmc5883l_exit);
MODULE_LICENSE("GPL");
