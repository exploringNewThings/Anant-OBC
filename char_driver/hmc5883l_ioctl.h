/*IOCTL parameters*/

#define HMC5883L_MAGIC '0xF2'
#define HMC5883L_READ _IOR(HMC5883L_MAGIC, 1, unsigned short)
#define HMC5883L_GET_MODE _IOR(HMC5883L_MAGIC, 2, unsigned short)
#define HMC5883L_GET_SAMPLE _IOR(HMC5883L_MAGIC, 3, unsigned short)
#define HMC5883L_GET_GAIN _IOR(HMC5883L_MAGIC, 4, unsigned short)
#define HMC5883L_GET_MESURA _IOR(HMC5883L_MAGIC, 5, unsigned short)
#define HMC5883L_GET_OUT_RATE _IOR(HMC5883L_MAGIC, 6, unsigned short)
#define HMC5883L_SET_MODE _IOW(HMC5883L_MAGIC, 7, unsigned short)
#define HMC5883L_SET_SAMPLE _IOW(HMC5883L_MAGIC, 8, unsigned short)
#define HMC5883L_SET_GAIN _IOW(HMC5883L_MAGIC, 9, unsigned short)
#define HMC5883L_SET_MESURA _IOW(HMC5883L_MAGIC, 10, unsigned short)
#define HMC5883L_SET_OUT_RATE _IOW(HMC5883L_MAGIC, 11, unsigned short)

