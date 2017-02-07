/*userspcae IOCTL*/
#include<sys/ioctl.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdio.h>
#include<asm/types.h>
#include"hmc5883l_ioctl.h"

int main()
{
	int ret, i;
	__u16 reading[3];
	char axis = {'x', 'y', 'z'};
	int fd = open("/dev/hmc5883l-i2c", O_RDWR);
	if(fd < 0){
		printf("Cannot open devile file\n");
		exit(-1);
	}
	ret = ioctl(fd, HMC5883L_READ, reading);
	if(ret<0){
		printf("IOCTL failed.\n");
		exit(-1);
	}
	for( i = 0; i < 3; i++){
		printf("%c : %u\n",axis[i], reading[i]);
	}
	return 0;
}
