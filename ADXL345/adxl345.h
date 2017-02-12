

#define OFFSET_Y 0x1F
#define OFFSET_Z 0x20
#define THRESH_ACT 0x24
#define THRESH_INACT 0x25
#define DATA_START 0x32 // 6 registers, 2 per axis
#define ID_ADXL345 0xE5
#define FIFO_CTL 0x38
#define DATA_FORMAT 0x31
#define POWER_CTL 0x2D

#define ADXL_X_AXIS 0
#define ADXL_Y_AXIS 1
#define ADXL_Z_AXIS 2
#define AXIS 3

#define SENSOR_ID "adxl345"

#define ADXL345_MAGIC '0xF2'
#define ADXL345_READ _IOR(ADXL345_MAGIC, 1, unsigned short)
