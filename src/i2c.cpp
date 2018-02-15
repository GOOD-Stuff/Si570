/*
 * i2c.cpp
 *
 *  Created on: Jul 14, 2016
 *      Author: user
 */
#include "i2c.h"

static u8 Register_7   = 7;	  // Address to register 7
static u8 Register_135 = 135; // Address to register 135 - 5 bit is prevents interim frequency changes when writing RFREQ registers.
static u8 Register_137 = 137; // Address to register 137 - 4 bit is freezes the DSPLL so the frequency configuration can be modified.

static int EnableSwitch (int hDev, u8 Channel);
static int verification (int hDev, u8 *CurrentRegister);
static int FastWriteRegs(int hDev, u8 *WriteBuff, int count, u8 Register);

/**
 * @brief  Setting Slave address of Si570 and including switch
 *
 * @param  hDev 	  - handle of device (I2C)
 *         Slave_Addr - Slave address of switch
 *
 * @return 1 - if was error
 * 		   0 - if address and switch successfully enabled
 */
int Init(int hDev, u8 Channel, u8 Slave_Addr) {
	int Status = 0;

	Status = EnableSwitch(hDev, Channel);
	if (Status != 0)
		return 1;
	
	Status = ioctl(hDev, I2C_SLAVE_FORCE, Slave_Addr);
	if (Status < 0) {
		printf("Can't setup device -\t%s\r\n", strerror(errno));
		return 1;
	}

	return 0;
}

/**
 * @brief  Enabling switch (TCA9548) and set channel
 *
 * @param  hDev - handle of device (I2C)
 *
 * @return 1	- if was error
 * 		   0 	- if switch successfully enabled
 */
static int EnableSwitch(int hDev, u8 Channel) {
	int Status = 0;

	Status = ioctl(hDev, I2C_SLAVE_FORCE, I2C_SLAVE_ADDR_SWITCH);
	if (Status < 0) {
		printf("Can't setup switch -\t%s\r\n", strerror(errno));
		return 1;
	}
	// Setup channel into switcher
	//u8 Channel = 0x21; // 1 and 5 channel (switch)
	if (write(hDev, &Channel, 1) != 1) {
		printf("Can't write into switcher\r\n");
		return 1;
	}
	usleep(1);
	
	return 0;
}

/**
 * @brief  Verification registers
 *
 * @param  hDev 		   - handle of device (I2C)
 *         CurrentRegister - array which contains current registers for Si570
 *
 * @return 1			   - if registers doesn't match
 * 		   0 			   - if registers match
 */
static int verification(int hDev, u8 *CurrentRegister) {
	int Status, i;
	u8 Buffer[6] = {0};

	Status = ReadRegs(hDev, Buffer, 6, Register_7);
	if (Status != 0) {
		printf("Something goes wrong in verification() - \t");
		return 1;
	}

	for (i = 0; i < 6; i++) {
		if (Buffer[i] != CurrentRegister[i])
			return 1;
	}

	return 0;
}

/**
 * @brief  Read registers from device
 *
 * @param  hDev 	- handle of device (I2C)
 * 		   ReadBuff - this array receive bytes from device
 * 		   count	- number of bytes
 * 		   Register	- register, which begins with reading (Si570)
 *
 * @return 1 		- if couldn't read
 * 		   0		- if read was successful
 */
int ReadRegs(int hDev, u8 *ReadBuff, int count, u8 Register) {
	u8 Status;
	u8 i;

	for (i = 0; i < count; i++) {
		Status = i2c_smbus_read_byte_data(hDev, Register);
		if (Status < 0) {
			printf("Can't read from Si570\r\n");
			return 1;
		}
		else
			ReadBuff[i] = Status;

		usleep(DELAY);
		Register++;
	}

	return 0;
}

/**
 * @brief  Write registers into device
 *
 * @param  hDev 	 - handle of device (I2C)
 * 		   WriteBuff - array which contains bytes for writing
 * 		   count	 - number of bytes
 * 		   Register	 - register, which begins with writing (Si570)
 *
 * @return 1  		 - if couldn't write
 * 		   0 		 - if write was successful
 */
int WriteRegs(int hDev, u8 *WriteBuff, int count, u8 Register) {
	u64 Status;
	u8 i;

	for (i = 0; i < count; i++) {
		Status = i2c_smbus_write_byte_data(hDev, Register, WriteBuff[i]);
		usleep(DELAY);
		Register++;
	}

	return 0;
}

/**
 * @brief  Write registers into device without delay
 *
 * @param  hDev 	 - handle of device (I2C)
 * 		   WriteBuff - array which contains bytes for writing
 * 		   count	 - number of bytes
 * 		   Register	 - register, which begins with writing (Si570)
 *
 * @return 1  		 - if couldn't write
 * 		   0 		 - if write was successful
 */
static int FastWriteRegs(int hDev, u8 *WriteBuff, int count, u8 Register) {
	u64 Status;
	u8 i;

	for (i = 0; i < count; i++) {
		Status = i2c_smbus_write_byte_data(hDev, Register, WriteBuff[i]);
		Register++;
	}

	return 0;
}

/**
 * @brief  Calculate registers HS_new N1 and RFREQ for manual writing into Si570
 *
 * @param  hDev 	 - handle of device (I2C)
 * 		   RegBuffer - array which contains current registers (Si570)
 * 		   Freq_new  - new frequency
 *
 * @return none
 */
void CalculateReg(int hDev, u8 *RegBuffer, float Freq_new) {
	int Status, i;
    u8 Buffer[5];				// Buffer which translate from RegBuffer to RFREQ presentation
    u8 HS;						// divider HS_div
    u8 N1;						// divider N1
    float FDCO_MAX = 5670;		// Max DCO frequency in Si570
    float FDCO_MIN = 4850;		// Min DCO frequency in Si570
    double fRFREQ;				// double representation of unsigned RFREQ
    u64 RFREQ = 0;				// unsigned 64 bit register RFREQ

    HS  = (RegBuffer[0] >> 5) & 0x7;
    N1  = (RegBuffer[0] & 0x1F) << 2;
    N1 |= (RegBuffer[1] & 0xF0) >> 6;
    N1 += 1;
    if      (HS == 0) HS = 4;
    else if (HS == 1) HS = 5;
    else if (HS == 2) HS = 6;
    else if (HS == 3) HS = 7;
    else if (HS == 5) HS = 9;
    else if (HS == 7) HS = 11;

    for (i = 0; i < 5; i++) {
 	   Buffer[i]  = (RegBuffer[i+1] << 4) & 0xF0;
	   Buffer[i] |= ((RegBuffer[i+2] & 0xF0) >> 4);
    }
    Buffer[4] = Buffer[4] >> 4;

    // Translate Buffer into RFREQ
    for (i = 0; i < 4; i++) {
	   RFREQ |= Buffer[i];
	   if (Buffer[i] == Buffer[3]) {
		   RFREQ = RFREQ << 4;
		   RFREQ |= Buffer[i+1];
	   }
	   else
		   RFREQ = RFREQ << 8;
    }
    fRFREQ = RFREQ / POW_2_28;
    const float Freq_cur = 56.32;
    double Fxtal = (Freq_cur * HS * N1) / fRFREQ;

    // Getting new dividers (HSn, N1n) for Freq_new
    u8 N1n;          // Output divider that is modified and used in calculating the new RFREQ
    u8 HSn;          // Output divider that is modified and used in calculating the new RFREQ
    int validCombo;  // Flag that is set to 1 if a valid combination of N1 and HS_DIV is found

    u16 divider_max; // Maximum divider for HS_DIV and N1 combination
    u16 curr_div;    // Minimum divider for HS_DIV and N1 combination
    float curr_n1;   // Used to calculate the final N1 to send to the Si570
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
		 if(n1_tmp == 0.0) {                    // Then curr_n1 is an integer
			N1n = (unsigned char) curr_n1;

			if ((N1n == 1) || ((N1n & 1) == 0)) // Then the calculated N1 is
				validCombo = 1;                 // either 1 or an even number
		 }
		 if (validCombo == 1) break;            // Divider was found, exit loop
	  } // end for
	  if (validCombo == 1) break;               // Divider was found, exit loop

	  curr_div = curr_div + 1;                  // If a valid divider is not found,
											    // increment curr_div and loop
    }

    fRFREQ = Freq_new * HSn * N1n;
    fRFREQ = fRFREQ / Fxtal;						 // Here comes the calculation error
    fRFREQ = round( fRFREQ * 100000000) / 100000000; // Translations in 8 decimal places
    fRFREQ = fRFREQ * POW_2_28;
    RFREQ  = fRFREQ;

    // Translate results into register for Si570
    if      (HSn == 11) HSn = 0xE0;
    else if (HSn == 9)  HSn = 0xA0;
    else if (HSn == 7)  HSn = 0x60;
    else if (HSn == 6)  HSn = 0x40;
    else if (HSn == 5)  HSn = 0x20;
    else if (HSn == 4)  HSn = 0x00;
    N1n = N1n - 1;

	u8 NewRegs[6];
	NewRegs[0] = N1n >> 2;
	NewRegs[0] |= HSn;
	NewRegs[1] = N1n << 6;
	NewRegs[5] = RFREQ;

	for (i = 4; i > 1; i--) {
		NewRegs[i] = RFREQ >> 8;
	    RFREQ      = RFREQ >> 8;
	}
	RFREQ       = RFREQ >> 8;
	NewRegs[1] |= RFREQ;

	// Read the current state of Register 137 and set the Freeze DCO bit in that register
	// This must be done in order to update Registers 7-12 on the Si57x
	Buffer[0] = 0x10;
	WriteRegs(hDev, Buffer, 1, Register_137);

	// Write the new values to Registers 7-12
	Status = WriteRegs(hDev, NewRegs, 6, Register_7);

	// Read the current state of Register 137 and clear the Freeze DCO bit
	Status = ReadRegs(hDev, Buffer, 1, Register_137);
	Buffer[0] = Buffer[0] & 0xEF;
	Status = FastWriteRegs(hDev, Buffer, 1, Register_137);

	// Set the NewFreq bit to alert the DPSLL that a new frequency configuration
	// has been applied
	Buffer[0] = 0x40;
	Status = FastWriteRegs(hDev, Buffer, 1, Register_135);
	usleep(DELAY);
    // for (i = 0; i < DELAY; i++);	// delay must be ~10 ms

	// Check whether the frequency has been written?
	Status = verification(hDev, NewRegs);
	if (Status != 0)
	    printf("\r\nThe frequency is not recorded \n\r" \
	    	   "Return to initial (~56.352 MHz) frequency\n\r");
}
