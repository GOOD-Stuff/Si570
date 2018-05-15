/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xparameters.h"
#include "xil_printf.h"
#include "xiicps.h"
#include <stdlib.h>
#include <math.h>
#include "xuartps_hw.h"

#define I2C_DEV_ID      	 XPAR_XIICPS_0_DEVICE_ID
#define I2C_BASE_ADDR	     XPAR_XIICPS_0_BASEADDR
#define I2C_SLAVE_ADDR_SWCH  0x70				// I2C address of switch (tca9548apwr)
#define I2C_SLAVE_ADDR_PLL   0x55				// I2C address of DSPLL  (Si570 - 570CAC000141DG)
#define I2C_SLAVE_ADDR_EROM  0x50				// I2C address of EEPROM (M24C02)
#define SIZE_wr		     17
#define SIZE_rd		     8
#define I2C_CLK		     100000//400000
#define DELAY		     1000000
#define CH_01_05	     0x21					// 1 and 5 channel (switch)
#define CH_06 		     0x40 					// 6 channel
#define POW_2_28             268435456.0    		// double precision floating point


#define CH_CARR_PLL			0x80 // 7 channel - PLL on Carrier board
#define CH_FMC_PLL			0x01 // 0 channel - PLL on Mezzanine board A


int i;
u8 WriteBuff[SIZE_wr];
u8 ReadBuff[SIZE_rd];
u8 DCO[2];
typedef u8 Address;
Address Register_7 = 7;	     // Address to register 7
Address Register_135 = 135;  // Address to register 135 - 5 bit is prevents interim frequency changes when writing RFREQ registers.
Address Register_137 = 137;  // Address to register 137 - 4 bit is freezes the DSPLL so the frequency configuration can be modified.

// ------------ PROTOTYPES ------------
static int  i2cInit	  (XIicPs *I2C);
static int  i2cConnect	  (XIicPs *I2C, u8 WriteBuffer,  int count);
static int  i2cDisconnect (XIicPs *I2C, u8 *WriteBuffer, int count);
static int  i2cWrite 	  (XIicPs *I2C, u8 *WriteBuffer, int count, u16 Slave_Addr, int Show);
static int  i2cFastWrite  (XIicPs *I2C, u8 *WriteBuffer, int count, u16 Slave_Addr, int Show);
static int  i2cRead 	  (XIicPs *I2C, u8 *ReadBuffer,  int count, Address i2cAddr, u16 Slave_Addr, int Show);
static int  i2cReadReg 	  (XIicPs *I2C, u8 *ReadBuffer,  int count);
static int  verification  (XIicPs *I2C, u8 *CurrentRegister);
void        CalculateReg  (u8 *RegBuffer, double Freq_new, XIicPs *I2C);
double      Input 	  ();
extern void outbyte	  (char c);
extern char inbyte 	  ();



int main() {
    init_platform();

    print("Hello World\n\r");

    int Status;
    Address i2cAddr;				// 'Control Register' for switcher, enable 1 and 5 channel
    Address EepromAddr = 0;			// Address of eeprom memory

    XIicPs MyI2C;
    memset(ReadBuff, 0, sizeof(int) * SIZE_rd);

    double start_freq;				// initial frequency ~56.352
    u8 freq = 3;
    if (freq == 1) {
	// 155.52 MHz
	start_freq = 155.52;
	WriteBuff[0] = Register_7;		 // 7 register
	WriteBuff[1] = 0x01;
	WriteBuff[2] = 0xC2;
	WriteBuff[3] = 0xB8;
	WriteBuff[4] = 0xBB;
	WriteBuff[5] = 0xE4;
	WriteBuff[6] = 0x72;
    }
    else if (freq == 2) {
	// 156.25 MHz
	start_freq = 156.25;
	WriteBuff[0] = Register_7; 		// 7 register
	WriteBuff[1] = 0x01;
	WriteBuff[2] = 0xC2;
	WriteBuff[3] = 0xBC;
	WriteBuff[4] = 0x01;
	WriteBuff[5] = 0x1E;
	WriteBuff[6] = 0xB8;
    }
    else if (freq == 3) {
	// 56.32 MHz
	start_freq = 161.1328125;
	WriteBuff[0] = Register_7;		// 7 register
	WriteBuff[1] = 0x01;
	WriteBuff[2] = 0xC2;
	WriteBuff[3] = 0xD1;
	WriteBuff[4] = 0xE1;
	WriteBuff[5] = 0x27;
	WriteBuff[6] = 0x88;
    }
    else {
	//* 100 MHz
	start_freq = 100;
	WriteBuff[0] = Register_7; 		// 7 register
	WriteBuff[1] = 0x22;
	WriteBuff[2] = 0x42;
	WriteBuff[3] = 0xBC;
	WriteBuff[4] = 0x01;
	WriteBuff[5] = 0x1E;
	WriteBuff[6] = 0xB8;
    }

	// Initialization IIC device
	print("\n\r*****\tInit\t*****\n\r");
	Status = i2cInit(&MyI2C);
	if (Status != XST_SUCCESS) {
	    print("Something goes wrong \n\r");
	    return XST_FAILURE;
	}
	print("*****\tSelfTest was good\t*****\n\r");

	// Connect to 6 channel on switcher
	i2cAddr = CH_06;
	print("\n\r*****\tConnecting to switcher\t*****\n\r");
	Status = i2cConnect(&MyI2C, i2cAddr, 1);
	if (Status != XST_SUCCESS) {
	    print("Something goes wrong in Connect \n\r");
	    return XST_FAILURE;
	}

	print("*****\tConnect was good\t*****\n\r");
	char c;
	while (1) {
		print("\n\n\n\n-----------------------------------------------------------------\r\n");
		print("------------------- WELCOME -------------------------------------\r\n");
		print("-----------------------------------------------------------------\r\n");
		print(" Select one of the options below:\r\n");
		print(" ## Read Data ##\r\n");
		print("    'a' - from Si570\r\n");
		print("    'b' - from EEPROM\r\n");
		print("    'c' - from switcher\r\n");
		print(" ## Write Data ##\r\n");
		print("    'd' - in Si570 (manual)\r\n");
		print("    'f' - in EEPROM\r\n");
		print("    'g' - in Si570 (auto)\r\n");
		print("    'h' - in switcher\r\n");
		print("    'x' - exit\r\n");

		print("\n\rOption Selected : ");
		c = inbyte();
		while ((c == '\r') || (c == '\n'))
		    c = inbyte();

		outbyte(c);
		print("\r\n");

		if ((c == 'a') || (c == 'A')) {
			// Reading freq from DSPLL
			print("\n\r*****\tReading from DSPLL\t*****\n\r");
			Status = i2cRead(&MyI2C, ReadBuff, 6, Register_7, I2C_SLAVE_ADDR_PLL, 1);
			if (Status != XST_SUCCESS) {
				print("Something goes wrong in Read \n\r");
				return XST_FAILURE;
			}
			print("*****\tRead was good\t*****\n\r");
		}
		else if (( c == 'b' ) || ( c == 'B')) {
			// Reading data from EEPROM
			print("\n\r*****\tReading from EEPROM\t*****\n\r");
			Status = i2cRead(&MyI2C, ReadBuff, SIZE_rd, EepromAddr, I2C_SLAVE_ADDR_EROM, 1);
			if (Status != XST_SUCCESS) {
				print("Something goes wrong in Read \n\r");
				return XST_FAILURE;
			}
			print("*****\tRead was good\t*****\n\r");
		}
		else if ((c == 'c') || (c == 'C')) {
			// Read from register of switcher
			print("\n\r*****\tRead registers from switcher\t*****\n\r");
			Status = i2cReadReg(&MyI2C, ReadBuff, 1);
			if (Status != XST_SUCCESS) {
				print("Something goes wrong in ReadReg \n\r");
				return XST_FAILURE;
			}
			print("*****\tRead was good\t*****\n\r");
		}

		else if ((c == 'd') || (c == 'D')) { // Set up frequency
			// Writes 0x01 to register 135. This will recall NVM bits into RAM.
			u8 tmpBuffer[2];
			tmpBuffer[0] = 135;
			tmpBuffer[1] = 0x01;
			Status = i2cWrite(&MyI2C, tmpBuffer, 2, I2C_SLAVE_ADDR_PLL, 0);
			if (Status != XST_SUCCESS) {
				print("Something goes wrong when you try to write new freq:\r\n");
				return XST_FAILURE;
			}

			// Reading freq from DSPLL
			print("\r\nStart-up Register Configuration:\r\n");
			Status = i2cRead(&MyI2C, ReadBuff, 6, Register_7, I2C_SLAVE_ADDR_PLL, 1);
			if (Status != XST_SUCCESS) {
				print("Something goes wrong in Read \n\r");
				return XST_FAILURE;
			}

		//	print("Please, insert your current frequency: ");
		//	float Freq_cur = Input();
			print("\r\nInsert your new frequency: ");
			double Freq_new = Input();
			CalculateReg(ReadBuff, Freq_new, &MyI2C);

		}
		else if ((c == 'f') || (c == 'F')) {
			// Preparation data to EEPROM
			for(i = 0; i < SIZE_wr; i++)
				WriteBuff[i] = i;

			WriteBuff[0] = EepromAddr;	// set the eeprom memory address

			// Write data in EEPROM
			print("\n\r*****\tWriting to EEPROM\t*****\n\r");
			Status = i2cWrite(&MyI2C, WriteBuff, SIZE_wr, I2C_SLAVE_ADDR_EROM, 1);
			if (Status != XST_SUCCESS) {
				print("Something goes wrong in Write \n\r");
				return XST_FAILURE;
			}
			print("*****\tWrite was good\t*****\n\r");
		}
		else if (( c == 'g') || ( c == 'G')) {
		    u8 Buffer[2];
		    int for_print = start_freq;
		    int precision = (start_freq - for_print) * 1000;
		    xil_printf("\r\nWill be written %d.%3d MHz \r\n", for_print, precision);
		    // Write data in DSPLL
		    print("\n\r*****\tWriting to DSPLL\t*****\n\r");
		    // Read the current state of Register 137 and set the Freeze DCO bit in that register
		    // This must be done in order to update Registers 7-12 on the Si57x
		    Buffer[1] = 0x10;
		    Buffer[0] = Register_137;
		    Status    = i2cWrite(&MyI2C, Buffer, 2, I2C_SLAVE_ADDR_PLL, 0);
		    if (Status != XST_SUCCESS) {
		 	   print("Write wasn't good :(\r\n");
		    }

		    // Write the new values to Registers 7-12
		    Status = i2cWrite(&MyI2C, WriteBuff, 7, I2C_SLAVE_ADDR_PLL, 1);
		    if (Status != XST_SUCCESS) {
			   print("Write wasn't good :(\r\n");
		    }

		    // Read the current state of Register 137 and clear the Freeze DCO bit
		    i2cRead(&MyI2C, Buffer, 1, Register_137, I2C_SLAVE_ADDR_PLL, 0);
		    Buffer[1] = Buffer[0] & 0xEF;
		    Buffer[0] = Register_137;
		    Status    = i2cFastWrite(&MyI2C, Buffer, 2, I2C_SLAVE_ADDR_PLL, 0);
		    if (Status != XST_SUCCESS) {
			   print("Write wasn't good :(\r\n");
		    }

		    // Set the NewFreq bit to alert the DPSLL that a new frequency configuration has been applied
		    Buffer[1] = 0x40;
		    Buffer[0] = Register_135;
		    Status = i2cFastWrite(&MyI2C, Buffer, 2, I2C_SLAVE_ADDR_PLL, 0);
		    if (Status != XST_SUCCESS) {
			   print("Write wasn't good :(\r\n");
		    }
		    for(i = 0; i < DELAY; i++);	// delay must be ~10 ms

		    // Check whether the frequency has been written?
		    print("\n\r*****\tVerification\t*****\n\r");
		    Status = verification(&MyI2C, WriteBuff);
		    if (Status == XST_SUCCESS) {
			   print("*****\tVerification was good\t*****\n\r");
		    } else print("\r\nThe frequency is not recorded \n\rReturn to initial (~56.352 MHz) frequency\n\r");
			print("*****\tWrite was good\t*****\n\r");
		}
		else if (( c == 'h') || (c == 'H')) {
			// Write data in switcher
			print("\n\r*****\tWriting to switcher\t*****\n\r");
			Status = i2cWrite(&MyI2C, WriteBuff, 1, I2C_SLAVE_ADDR_SWCH, 1);
			if (Status != XST_SUCCESS) {
				print("Something goes wrong in Write \n\r");
				return XST_FAILURE;
			}
			print("*****\tWrite was good\t*****\n\r");
		}
		else if ((c == 'x') || (c == 'X')) {
			break;
		}
	}

	// Disconnect
	i2cAddr = 0;
	print("\n\r*****\tDisconnect from switcher\t*****\n\r");
	Status = i2cDisconnect(&MyI2C,(u8*) &i2cAddr, 1);
	if (Status != XST_SUCCESS) {
		print("Something goes wrong in ReadReg \n\r");
		return XST_FAILURE;
	}
	print("*****\tDisconnect was good\t*****\n\r");

	// Read from register of switcher
	print("\n\r*****\tRead registers from switcher\t*****\n\r");
	Status = i2cReadReg(&MyI2C, ReadBuff, 1);
	if (Status != XST_SUCCESS) {
		print("Something goes wrong in ReadReg \n\r");
		return XST_FAILURE;
	}
	print("*****\tRead was good\t*****\n\r");

	cleanup_platform();
	return XST_SUCCESS;
}

/*
 * @brief  The getting of the values from the UART
 *
 * @param  none
 *
 * @return result - float's value from the UART
 */
double Input(){
	char str[80];
	for (i = 0; i < 80; i++)
		str[i] = 0;
	char symbol;

	symbol = inbyte();
	while ((symbol == '\r') || (symbol == '\n'))
		symbol = inbyte();

    str[0] = symbol;
	outbyte(symbol);
	for (i = 0; i < DELAY; i++);

	i = 1;
	while (XUartPs_IsReceiveData(STDIN_BASEADDRESS)) {
		symbol = inbyte();
		if ((symbol != '\r') && (symbol != '\n')) {
			str[i] = symbol;
			i++;
		}
		outbyte(symbol);
	}
	double result = 0;
	result = atof(str);
	return result;
}

/*
 * @brief  Calculate and write new RFREQ, HS_DIV and N1 for Si570
 *
 * @param  RegBuffer - Current registers stored in Si570 RAM
 *         Freq_new  - New frequency
 *         I2C       - Instance pointer of I2C
 *
 * @return none
 */
void CalculateReg(u8 *RegBuffer, double Freq_new, XIicPs *I2C){
	   int Status;
	   u8 Buffer[5];			// Buffer which translate from RegBuffer to RFREQ presentation
	   u8 HS;				// divider HS_div
	   u8 N1;				// divider N1
	   float FDCO_MAX = 5670;		// Max DCO frequency in Si570
	   float FDCO_MIN = 4850;		// Min DCO frequency in Si570
	   double fRFREQ;			// double representation of unsigned RFREQ
	   u64 RFREQ = 0;			// unsigned 64 bit register RFREQ

	   // Get HS_div and N1 from current RFREQ registers
	   HS  = (RegBuffer[0] >> 5) & 0x7;
	   N1  = (RegBuffer[0] & 0x1F) << 2;
	   N1 |= (RegBuffer[1] & 0xF0) >> 6;
	   N1 += 1;
	   if	   (HS == 0) HS = 4;
	   else if (HS == 1) HS = 5;
	   else if (HS == 2) HS = 6;
	   else if (HS == 3) HS = 7;
	   else if (HS == 5) HS = 9;
	   else if (HS == 7) HS = 11;
	   xil_printf("\n\r\nHS: %d; N1: %d; \n\r", HS, N1);

	   for(i = 0; i < 5; i++){
		   Buffer[i] = (RegBuffer[i+1] << 4) & 0xF0;
		   Buffer[i] |= (( RegBuffer[i+2] & 0xF0) >> 4);
	   }
	   Buffer[4] = Buffer[4] >> 4;

	   // Translate Buffer into RFREQ
	   print("\r\nRFREQ:\r\n");
	   for (i = 0; i < 4; i++) {
		   xil_printf("RFREQ %d = 0x%02x \r\n", i+1, Buffer[i]);
		   RFREQ |= Buffer[i];
		   if (Buffer[i] == Buffer[3]) {
			   RFREQ = RFREQ << 4;
			   RFREQ |= Buffer[i+1];
			   xil_printf("RFREQ %d = 0x%02x \r\n", i+1, Buffer[i+1]);
		   }
		   else
			   RFREQ = RFREQ << 8;
	   }
	   fRFREQ = RFREQ / POW_2_28;
	   //double Fxtal = 114.285; // <- because i always get this number. Formula is ( Freq_cur * HS * N1 ) / fRFREQ;
	   const double Freq_cur = 100;//56.32;
	   double Fxtal = (Freq_cur * HS * N1) / fRFREQ;

	   // Getting new dividers (HSn, N1n) for Freq_new
	   u8 N1n;                        // Output divider that is modified and used in calculating the new RFREQ
	   u8 HSn;                     	  // Output divider that is modified and used in calculating the new RFREQ
	   int validCombo;                // Flag that is set to 1 if a valid combination of N1 and HS_DIV is found

	   u16 divider_max;               // Maximum divider for HS_DIV and N1 combination
	   u16 curr_div;                  // Minimum divider for HS_DIV and N1 combination
	   float curr_n1;                 // Used to calculate the final N1 to send to the Si570
	   float n1_tmp;

	   // Find dividers (get the max and min divider range for the HS_DIV and N1 combo)
	   divider_max = floorf(FDCO_MAX / Freq_new);
	   curr_div    = ceilf(FDCO_MIN / Freq_new);
	   validCombo  = 0;
	   u8 HS_DIV[] = {11, 9, 7, 6, 5, 4};
	   while (curr_div <= divider_max) {
	      //check all the HS_DIV values with the next curr_div
	      for (i = 0; i < 6; i++) {
	         // get the next possible n1 value
	         HSn = HS_DIV[i];
	         curr_n1 = (float)(curr_div) / (float)(HSn);

	         // Determine if curr_n1 is an integer and an even number or one
	         // then it will be a valid divider option for the new frequency
	         n1_tmp = floorf(curr_n1);
	         n1_tmp = curr_n1 - n1_tmp;
	         if (n1_tmp == 0.0) {                     // Then curr_n1 is an integer
	            N1n = (unsigned char) curr_n1;

	            if ((N1n == 1) || ((N1n & 1) == 0)) { // Then the calculated N1 is
	            	validCombo = 1;                   // either 1 or an even number
	            }
	         }
	         if (validCombo == 1) break;            // Divider was found, exit loop
	      }
	      if (validCombo == 1) break;               // Divider was found, exit loop

	      curr_div = curr_div + 1;                 // If a valid divider is not found,
	                                               // increment curr_div and loop
	   }
	   xil_printf("\r\nHSn = %d; N1n = %d \r\n", HSn, N1n);

	   fRFREQ = Freq_new * HSn * N1n;
	   fRFREQ = fRFREQ / Fxtal;									// Here comes the calculation error
	   fRFREQ = round( fRFREQ * 100000000 ) / 100000000;		// Translations in 8 decimal places
	   fRFREQ = fRFREQ * POW_2_28;
	   RFREQ = fRFREQ;

	   // Translate results into register for Si570
	   if      (HSn == 11) HSn = 0xE0;
	   else if (HSn == 9)  HSn = 0xA0;
	   else if (HSn == 7)  HSn = 0x60;
	   else if (HSn == 6)  HSn = 0x40;
	   else if (HSn == 5)  HSn = 0x20;
	   else if (HSn == 4)  HSn = 0x00;
	   N1n = N1n - 1;

	   u8 WriteRegs[7];
	   WriteRegs[0] = Register_7;
	   WriteRegs[1] = N1n >> 2;
	   WriteRegs[1] |= HSn;
	   WriteRegs[2] = N1n << 6;
	   WriteRegs[6] = RFREQ;

	   for(i = 5; i > 2; i--){
		  WriteRegs[i] = RFREQ >> 8;
		  RFREQ        = RFREQ >> 8;
	   }
	   RFREQ = RFREQ >> 8;
	   WriteRegs[2] |= RFREQ;

	   print("\r\nNew Register Configuration:\r\n");
	   for (i = 0; i < 6; i++)
	 	  xil_printf("Register %d = 0x%02x \r\n", i+7, WriteRegs[i+1]);
	   print("\r\n");

	   // Read the current state of Register 137 and set the Freeze DCO bit in that register
	   // This must be done in order to update Registers 7-12 on the Si57x
	   Buffer[1] = 0x10;
	   Buffer[0] = 137;
	   Status = i2cWrite(I2C, Buffer, 2, I2C_SLAVE_ADDR_PLL, 0);
	   if (Status != XST_SUCCESS) {
		   print("Write wasn't good :(\r\n");
	   }

	   // Write the new values to Registers 7-12
	   Status = i2cWrite(I2C, WriteRegs, 7, I2C_SLAVE_ADDR_PLL, 0);
	   if (Status != XST_SUCCESS) {
		   print("Write wasn't good :(\r\n");
	   }

	   // Read the current state of Register 137 and clear the Freeze DCO bit
	   i2cRead(I2C, Buffer, 1, Register_137, I2C_SLAVE_ADDR_PLL, 0);
	   Buffer[1] = Buffer[0] & 0xEF;
	   Buffer[0] = Register_137;
	   Status = i2cFastWrite(I2C, Buffer, 2, I2C_SLAVE_ADDR_PLL, 0);
	   if (Status != XST_SUCCESS) {
		   print("Write wasn't good :(\r\n");
	   }

	   // Set the NewFreq bit to alert the DPSLL that a new frequency configuration has been applied
	   Buffer[1] = 0x40;
	   Buffer[0] = Register_135;
	   Status = i2cFastWrite(I2C, Buffer, 2, I2C_SLAVE_ADDR_PLL, 0);
	   if (Status != XST_SUCCESS) {
		   print("Write wasn't good :(\r\n");
	   }
	   for (i = 0; i < DELAY; i++);	// delay must be ~10 ms

	   // Check whether the frequency has been written?
	   print("\n\r*****\tVerification\t*****\n\r");
	   Status = verification(I2C, WriteRegs);
	   if (Status == XST_SUCCESS) {
		   print("*****\tVerification was good\t*****\n\r");
	   } else print("\r\nThe frequency is not recorded \n\rReturn to initial (~56.352 MHz) frequency\n\r");
}

/*
 * @brief  Verification RFREQ values in Si570
 *
 * @param  I2C  		   - Instance pointer of I2C
 * 		   CurrentRegister - Pointer on expected registers
 *
 * @return XST_SUCCESS 	   - If CurrentRegister has been written and valid then return success
 */
static int verification(XIicPs *I2C, u8 *CurrentRegister){
	int Status;
	u8 Buffer[6];
	print("We read: \r\n");

	Status = i2cRead(I2C, Buffer, 6, Register_7, I2C_SLAVE_ADDR_PLL, 1);
	if (Status != XST_SUCCESS) {
		print("Something goes wrong in verification() - \t");
		return XST_FAILURE;
	}

	for (i = 0; i < 6; i++) {
		xil_printf("Buffer = 0x%02x vs Current = 0x%02x \n\r", Buffer[i], CurrentRegister[i+1]);
		if (Buffer[i] != CurrentRegister[i+1])
			return XST_FAILURE;
	}
	return XST_SUCCESS;
}

/*
 * @brief  Initialization I2C module in arm
 *
 * @param  I2C         - Instance pointer of I2C
 *
 * @return XST_SUCCESS - if function has been worked good then return success
 */
static int i2cInit(XIicPs *I2C) {
	int Status;
	XIicPs_Config *Cnfg;

	print("\tInitialization \n\r");

	Cnfg = XIicPs_LookupConfig(I2C_DEV_ID);
	if (Cnfg == NULL) {
		print("Failure in Lookup() - ");
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(I2C, Cnfg, Cnfg->BaseAddress);
	if (Status != XST_SUCCESS) {
		print("Failure in CfgInit() - \t");
		return XST_FAILURE;
	}

	Status = XIicPs_SelfTest(I2C);
	if (Status != XST_SUCCESS) {
		print("Failure in SelfTest() - ");
		return XST_FAILURE;
	}

	XIicPs_SetSClk(I2C, I2C_CLK);// Sets the serial clock rate for the IIC device

	return XST_SUCCESS;
}

/*
 * @brief  Set connection to switcher
 *
 * @param  I2C         - Instance pointer of I2C
 * 		   WriteBuffer - Pointer to data for write
 * 		   count       - number of bytes for write
 *
 * @return XST_SUCCESS - if function has been worked good then return success
 * 	  	   XST_FAILURE - if function has been worked bad then return fail
 */
static int i2cConnect(XIicPs *I2C, u8 WriteBuffer, int count){
	int Status;

	// Send 'Control Register' (first byte)
	Status = XIicPs_MasterSendPolled(I2C, &WriteBuffer, count, I2C_SLAVE_ADDR_SWCH);
	if (Status != XST_SUCCESS) {
		xil_printf("Failure in Send CR %d - ", Status);
		return XST_FAILURE;
	}
	while (XIicPs_BusIsBusy(I2C)) {}
	for (i = 0; i < DELAY; i++) {}

	return XST_SUCCESS;
}

/*
 * @brief  Disconnection from switcher
 *
 * @param  I2C         - Instance pointer of I2C
 * 		   WriteBuffer - Pointer to data for write
 * 		   count       - number of bytes for write
 *
 * @return XST_SUCCESS - if function has been worked good then return success
 * 	  	   XST_FAILURE - if function has been worked bad then return fail
 */
static int i2cDisconnect(XIicPs *I2C, u8 *WriteBuffer, int count){
	int Status;

	// Send 'Control Register' (first byte)
	Status = XIicPs_MasterSendPolled(I2C, WriteBuffer, count, I2C_SLAVE_ADDR_SWCH);
	if (Status != XST_SUCCESS) {
		print("Failure in Send CR - ");
		return XST_FAILURE;
	}
	while (XIicPs_BusIsBusy(I2C)) {}
	for (i = 0; i < DELAY; i++) {}

	return XST_SUCCESS;
}

/*
 * @brief  Write data from master to I2C slave
 *
 * @param  I2C         - Instance pointer of I2C
 * 		   WriteBuffer - Pointer to data for write
 * 		   count       - number of bytes for write
 * 		   Slave_Addr  - I2C slave address
 * 		   Show		   - if >= 1 then show output information
 *
 * @return XST_SUCCESS - if function has been worked good then return success
 * 	  	   XST_FAILURE - if function has been worked bad then return fail
 */
static int i2cWrite(XIicPs *I2C, u8 *WriteBuffer, int count, u16 Slave_Addr, int Show) {
	int Status;

	// Filling the buffer base address and data
	if (Show) {
		for (i = 0; i < count; i++) {
			xil_printf("WriteBuffer = 0x%04x \n\r", WriteBuffer[i]);
		}
	}
	// Send base address (first byte) and data (second byte)
	Status = XIicPs_MasterSendPolled(I2C, WriteBuffer, count, Slave_Addr);
	if (Status != XST_SUCCESS) {
		print("Failure in Send DSPLL - ");
		return XST_FAILURE;
	}
	while (XIicPs_BusIsBusy(I2C)) {}
	for (i = 0; i < DELAY; i++) {}

	return XST_SUCCESS;
}

/*
 * @brief  Write data from master to I2C slave
 *
 * @param  I2C         - Instance pointer of I2C
 * 		   WriteBuffer - Pointer to data for write
 * 		   count       - number of bytes for write
 * 		   Slave_Addr  - I2C slave address
 * 		   Show		   - if >= 1 then show output information
 *
 * @return XST_SUCCESS - if function has been worked good then return success
 * 	  	   XST_FAILURE - if function has been worked bad then return fail
 * @note
 * 		This function is different from the 'i2cWrite' in that i2cFastWrite doesn't have the delay
 */
static int i2cFastWrite(XIicPs *I2C, u8 *WriteBuffer, int count, u16 Slave_Addr, int Show){
	int Status;

	// Filling the buffer base address and data
	if (Show) {
		for(i = 0; i < count; i++) {
			xil_printf("WriteBuffer = 0x%04x \n\r", WriteBuffer[i]);
		}
	}
	// Send base address (first byte) and data (second byte)
	Status = XIicPs_MasterSendPolled(I2C, WriteBuffer, count, Slave_Addr);
	if (Status != XST_SUCCESS) {
		print("Failure in Send DSPLL - ");
		return XST_FAILURE;
	}
	while (XIicPs_BusIsBusy(I2C)) {}

	return XST_SUCCESS;
}


/*
 * @brief  Read registers from switcher
 *
 * @param  I2C        - Instance pointer of I2C
 * 		   ReadBuffer - Pointer to data for read
 * 		   count      - number of bytes for write
 *
 * @return XST_SUCCESS - if function has been worked good then return success
 * 	  	   XST_FAILURE - if function has been worked bad then return fail
 */
static int i2cReadReg(XIicPs *I2C, u8 *ReadBuffer, int count){
	int Status;
	// Receive data from register
	Status = XIicPs_MasterRecvPolled(I2C, ReadBuffer, count, I2C_SLAVE_ADDR_SWCH);
	if (Status != XST_SUCCESS) {
		print("Failure in Send - ");
		return XST_FAILURE;
	}
	while (XIicPs_BusIsBusy(I2C)) {	}
	for (i = 0; i < DELAY; i++) {	}

	for (i = 0; i < count; i++)
		xil_printf("%d) Rd: 0x%04x \n\r", i + 1, ReadBuffer[i]);

	return XST_SUCCESS;
}

/*
 * @brief  Read data from I2C slave to master
 *
 * @param  I2C 		  - Instance pointer of I2C
 * 		   ReadBuffer - Pointer to data for read
 * 		   count 	  - Number of bytes for write
 * 		   i2cAddr 	  - Base address in slave
 * 		   Slave_Addr - I2C slave address
 * 		   Show		  - if >= 1 then show output information
 *
 * @return XST_SUCCESS - if function has been worked good then return success
 * 	  	   XST_FAILURE - if function has been worked bad then return fail
 */
static int i2cRead(XIicPs *I2C, u8 *ReadBuffer, int count, Address i2cAddr,	u16 Slave_Addr, int Show) {
	int Status;

	//Address EepromAddr = 0;
	// Set EEPROM address
	Status = XIicPs_MasterSendPolled(I2C, (u8*) &i2cAddr, 1, Slave_Addr);
	if (Status != XST_SUCCESS) {
		print("Failure in Send - ");
		return XST_FAILURE;
	}
	while(XIicPs_BusIsBusy(I2C)) {}
	for (i = 0; i < DELAY; i++) {}

	// Receive data from register
	Status = XIicPs_MasterRecvPolled(I2C, ReadBuffer, count, Slave_Addr);
	if (Status != XST_SUCCESS) {
		print("Failure in Read - ");
		return XST_FAILURE;
	}
	while (XIicPs_BusIsBusy(I2C)) {	}
	for (i = 0; i < DELAY; i++) {	}

	if (Show) {
		for (i = 0; i < count; i++)
			xil_printf("%d) Rd: 0x%04x \n\r", i + 1, ReadBuffer[i]);
	}
	return XST_SUCCESS;
}
