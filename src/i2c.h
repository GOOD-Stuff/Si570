/*
 * i2c.h
 *
 *  Created on: Jul 14, 2016
 *      Author: user
 */

#ifndef I2C_H_
#define I2C_H_

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <math.h>
#include "i2c-dev-user.h"
#include <linux/types.h>
#include <cerrno>
#include <cstring>

#define I2C_SLAVE_ADDR_SWITCH  0x74
#define POW_2_28               268435456.0 // double precision floating point
#define DELAY 			  	   10000       // 100000

typedef __u8  u8;
typedef __u16 u16;
typedef __u64 u64;

// IIC
int  Init        (int hDev, u8 Channel, u8 Slave_Addr);
int  ReadRegs    (int hDev, u8 *ReadBuff, int count, u8 Register);
int  WriteRegs   (int hDev, u8 *WriteBuff, int count, u8 Register);
// Si570
void CalculateReg(int hDev, u8 *RegBuffer, float Freq_new);


#endif /* I2C_H_ */
