/*IOCTL parameters*/

#include <asm/ioctl.h>
#define HMC5883L_MAGIC '0xF2'
#define HMC5883L_READ _IOR(HMC5883L_MAGIC, 1, int)
