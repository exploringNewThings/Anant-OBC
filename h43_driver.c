#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/delay.h>

#define SENSOR_ID_STRING "H43"
#define SENSOR_NAME "hmc5883l-i2c"

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

struct sensor_hmc5883l {
    struct mutex lock;
    struct i2c_client *client;
    u8 sample;
    u8 out_rate;
    u8 mesura;
    u8 mode;
    u8 gain;
    u16 axis[3];
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
       printk(KERN_ERR "HMC5883L: Invalid mode\n");
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
        printk(KERN_ERR "HMC5883L: Invalid gain\n");
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
        printk(KERN_ERR "HMC5883L: Invalid data out rate \n");
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

//Attribute methods start here
static ssize_t hmc5883l_int_x(struct device *dev, struct device_attribute *attr, char *buf){
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	hmc5883l_read_block();
	return sprintf(buf,"%d\n", hmc5883l->axis[0]);
}

static ssize_t hmc5883l_int_y(struct device *dev, struct device_attribute *attr, char *buf){
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", hmc5883l->axis[1]);
}


static ssize_t hmc5883l_int_z(struct device *dev, struct device_attribute *attr, char *buf){
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	return sprintf(buf,"%d\n", hmc5883l->axis[2]);
}

static ssize_t hmc5883l_mode_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 mode = simple_strtoul(buf, NULL, 10);
	printk(KERN_NOTICE "Before changing mode\n"); 
	int err = hmc5883l_set_mode(sensor_hmc5883l->client, mode);
	printk(KERN_NOTICE "After changing mode %d\n", err); 
	return count;
}

static ssize_t hmc5883l_mode_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 mode = 0;
	mode = hmc5883l_get_mode(sensor_hmc5883l->client);
	return sprintf(buf, "%u\n", mode);
}

static ssize_t hmc5883l_data_out_rate_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 data_rate = simple_strtoul(buf, NULL, 10); 
	hmc5883l_set_data_out_rate (sensor_hmc5883l->client, data_rate);
	return count;
}

static ssize_t hmc5883l_data_out_rate_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 data_out_rate = 0;
	data_out_rate = hmc5883l_get_data_out_rate(sensor_hmc5883l->client);
	return sprintf( buf, "%u\n", data_out_rate);
}

static ssize_t hmc5883l_sample_average_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 sample_average = simple_strtoul(buf, NULL, 10); 
	hmc5883l_set_sample_average( sensor_hmc5883l->client, sample_average);
	return count;
}

static ssize_t hmc5883l_sample_average_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 sample_average = 0;
	sample_average = hmc5883l_get_sample_average( sensor_hmc5883l->client);
	return sprintf( buf, "%u\n", sample_average);
}

static ssize_t hmc5883l_mesura_set( struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 mesura = simple_strtoul(buf, NULL, 10); 
	hmc5883l_set_mesura(sensor_hmc5883l->client, mesura);
	return count;
}

static ssize_t hmc5883l_mesura_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 mesura = 0;
	mesura = hmc5883l_get_mesura( sensor_hmc5883l->client);
	return sprintf( buf, "%u\n", mesura);
}


static ssize_t hmc5883l_gain_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 gain = simple_strtoul(buf, NULL, 10); 
	hmc5883l_set_gain( sensor_hmc5883l->client, gain);
	return count;
}

static ssize_t hmc5883l_gain_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_hmc5883l *sensor_hmc5883l = dev_get_drvdata(dev);
	u8 gain = 0;
	gain = hmc5883l_get_gain(sensor_hmc5883l->client);
	return sprintf( buf, "%u\n", gain);
}

//Attribute methods end here

//Attributes declared here
static DEVICE_ATTR(hmc5883l_int_x, 0664, hmc5883l_int_x, NULL);
static DEVICE_ATTR(hmc5883l_int_y, 0664, hmc5883l_int_y, NULL);
static DEVICE_ATTR(hmc5883l_int_z, 0664, hmc5883l_int_z, NULL);
static DEVICE_ATTR(hmc5883l_gain, 0664, hmc5883l_gain_get, hmc5883l_gain_set);
static DEVICE_ATTR(hmc5883l_mesura, 0664, hmc5883l_mesura_get, hmc5883l_mesura_set);
static DEVICE_ATTR(hmc5883l_data_out_rate, 0664, hmc5883l_data_out_rate_get, hmc5883l_data_out_rate_set);
static DEVICE_ATTR(hmc5883l_sample_average, 0664, hmc5883l_sample_average_get, hmc5883l_sample_average_set);
static DEVICE_ATTR(hmc5883l_mode, 0664, hmc5883l_mode_get, hmc5883l_mode_set); 

static struct device_attribute *hmc5883l_attr_list[] = {
	&dev_attr_hmc5883l_int_x,
	&dev_attr_hmc5883l_int_y,
	&dev_attr_hmc5883l_int_z,
	&dev_attr_hmc5883l_mode,
	&dev_attr_hmc5883l_data_out_rate,
	&dev_attr_hmc5883l_sample_average,
	&dev_attr_hmc5883l_mesura,
	&dev_attr_hmc5883l_gain,
};

static int hmc5883l_create_attr(struct device *dev){
	int index, err = 0;
	int num = (int)(sizeof(hmc5883l_attr_list)/sizeof(hmc5883l_attr_list[0]));
	if(dev == NULL){
		return -EINVAL;
	}

	for(index = 0; index < num; index++){
		err = device_create_file(dev, hmc5883l_attr_list[index]);
		if(err < 0){
			printk(KERN_DEBUG "HMC5883L: Error creating attribute file\n");
			break;
		}
	}
	return err;
}

static void hmc5883l_remove_attr(struct device *dev){
	int index, err = 0;
	int num = (int)(sizeof(hmc5883l_attr_list)/sizeof(hmc5883l_attr_list[0]));
	if(dev == NULL){
		return -EINVAL;
	}

	for(index = 0; index < num; index++){
		device_create_file(dev, hmc5883l_attr_list[index]);
		}
	
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
    hmc5883l_create_attr(&(hmc5883l->client->dev));
    return 0;
}

static int hmc5883l_remove(struct i2c_client *client)
{
    return 0;
}

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
        .name = SENSOR_NAME,
	.owner = THIS_MODULE,
	.of_match_table = hmc5883l_of_match,
    },
    .probe = hmc5883l_probe,
    .remove = hmc5883l_remove,
    .id_table = hmc5883l_id,
};

static int __init hmc5883l_init(void)
{
    hmc5883l = kzalloc(sizeof(*hmc5883l), GFP_KERNEL);
    if (!hmc5883l)
        return -ENOMEM;
    mutex_init(&hmc5883l->lock);
    i2c_add_driver(&hmc5883l_driver);
    return 0;
}


static void __exit hmc5883l_exit(void)
{
    hmc5883l_remove_attr(&(hmc5883l->client->dev));
    i2c_del_driver(&hmc5883l_driver);
    if (hmc5883l->client)
        i2c_unregister_device(hmc5883l->client);
    printk(KERN_DEBUG "HMC5883L: Module removed \n");
}

module_init(hmc5883l_init);
module_exit(hmc5883l_exit);
MODULE_LICENSE("GPL");
