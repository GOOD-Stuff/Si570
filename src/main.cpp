/*
 * clk_set.cpp
 *  Created on: Aug 2, 2016
 *      Author: user
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "i2c.h"
#define I2C_SLAVE_ADDR_PLL  0x55
#define CH_CARR_PLL			0x80 // 7 channel - PLL on Carrier board
#define CH_FMC_PLL			0x01 // 0 channel - PLL on Mezzanine board A
#define SIZE_wr			 	17
#define SIZE_rd				8

static u8 Register_7   = 7;	     // Address to register 7
static u8 Register_135 = 135;    // Address to register 135 - 5 bit is prevents interim frequency changes when writing RFREQ registers.
//static u8 Register_137 = 137;  // Address to register 137 - 4 bit is freezes the DSPLL so the frequency configuration can be modified.

u8 WriteBuff[SIZE_wr];
u8 ReadBuff[SIZE_rd];

int main(void) {
	int Status = 0;
	int hDev   = 0;

	const char *filename = "/dev/i2c-0";
	if ((hDev = open(filename, O_RDWR)) < 0) {
		printf("Can't connect to I2C\r\n");
		return 1;
	}

	// setup Si570 on Carrier board
	Status = Init(hDev, CH_CARR_PLL, I2C_SLAVE_ADDR_PLL);
	if (Status != 0) {
		printf("Something goes wrong\r\n");
		close(hDev);
		return 1;
	}

	float Freq_CAR_new = 156.25;	// initial frequency for Carrier board
	float Freq_FMC_new = 155.52;	// initial for Mezzanine board
	printf("Set Si570 on Carrier and Mezzanine board in %3.3f MHz and %3.3f MHz\r\n",
			Freq_CAR_new, Freq_FMC_new);

	WriteBuff[0] = 0x01;
	Status = WriteRegs(hDev, WriteBuff, 1, Register_135);
	if (Status != 0) {
		printf("Something goes wrong in Write to 135 reg\r\n");
		close(hDev);
		return 1;
	}
	Status = ReadRegs(hDev, ReadBuff, 6, Register_7);
	if (Status != 0) {
		printf("Something goes wrong in Read \n\r");
		close(hDev);
		return 1;
	}
	CalculateReg(hDev, ReadBuff, Freq_CAR_new);

	// setup Si570 on Mezzanine board
	Status = Init(hDev, CH_FMC_PLL, I2C_SLAVE_ADDR_PLL);
	if (Status != 0) {
		printf("Something goes wrong\r\n");
		close(hDev);
		return 1;
	}
	WriteBuff[0] = 0x01;
	Status = WriteRegs(hDev, WriteBuff, 1, Register_135);
	if (Status != 0) {
		printf("Something goes wrong in Write to 135 reg\r\n");
		close(hDev);
		return 1;
	}
	Status = ReadRegs(hDev, ReadBuff, 6, Register_7);
	if (Status != 0) {
		printf("Something goes wrong in Read \n\r");
		close(hDev);
		return 1;
	}
	CalculateReg(hDev, ReadBuff, Freq_CAR_new);

	close(hDev);
	return 0;
}



