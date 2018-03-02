/**********************************************************************
* $Id$		LCDTerm.c			2011-12-06
*//**
* @file		LCDTerm.c
* @brief	This is a library that can be used to display text on the LCD of Hitex 1800 board
* @version	1.0
* @date		06. Dec. 2011
* @author	NXP MCU SW Application Team
*
* Copyright(C) 2011, NXP Semiconductor
* All rights reserved.
*
***********************************************************************
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
* Permission to use, copy, modify, and distribute this software and its
* documentation is hereby granted, under NXP Semiconductors’
* relevant copyright in the software, without fee, provided that it
* is used in conjunction with NXP Semiconductors microcontrollers.  This
* copyright, permission, and disclaimer notice must appear in all copies of
* this code.
**********************************************************************/
#if defined(HITEX_LCD_TERM)
#include "lpc43xx_ssp.h"
#include "lpc43xx_scu.h"
#include "lpc43xx_cgu.h"
//#include "lpc43xx_libcfg.h"
#include "debug_frmwrk.h"
#include "lpc43xx_gpio.h"
#include "LCDTerm.h"


/************************** PRIVATE DEFINTIONS ************************/
/** Max buffer length */
#define BUFFER_SIZE				0x40
#define FONT_OFFSET     		32
#define FONT_WIDTH      		5
#define CHARS_PER_ROW			25
#define NO_OF_ROWS				4
#define MAX_CHARS_DISPLAYED		CHARS_PER_ROW * NO_OF_ROWS
#define ROW_PIXELS				128
#define COL_PIXELS				8
#define Highlight 				1
#define NoHighlight 			0

/************************** PRIVATE VARIABLES *************************/

// SSP Configuration structure variable
SSP_CFG_Type SSP_ConfigStruct;

// Tx buffer
uint8_t Tx_Buf[BUFFER_SIZE];

// Rx buffer
uint8_t Rx_Buf[BUFFER_SIZE];

//Character indexes
uint8_t Char_Index[256];
uint8_t Highlight_Value[256];

char Chars_Displayed[88];
uint8_t Number_of_Chars = 0, Start_Index = 0, Current_Index = 0;

/***************************IMPORT VARIABLES**************************/
extern const UNS_16 x5x7_bits [];

/************************** PRIVATE FUNCTIONS *************************/
void delay(unsigned int n)
{
   volatile unsigned int i,j;
   for (i=0;i<n;i++)
        for (j=0;j<400;j++)
           {;}
}

void data_out(unsigned char i, SSP_DATA_SETUP_Type *xferConfig);
void comm_out(unsigned char j, SSP_DATA_SETUP_Type *xferConfig);
void init_LCD(SSP_DATA_SETUP_Type *xferConfig);
void Init_Indexes(void);

/*-------------------------PRIVATE FUNCTIONS------------------------------*/

/*-------------------------SET UP FUNCTION------------------------------*/
/*********************************************************************//**
 * @brief		Main set up function
 * @param[in]	None
 * @return 		SSP_DATA_SETUP_Type
 **********************************************************************/
SSP_DATA_SETUP_Type *InitLCDTerm(void)
{
//	char * lcd_string = logo;
	unsigned char page = 0xB0;
	int i = 0, j = 0;
	static SSP_DATA_SETUP_Type xferConfig;

	Init_Indexes();

	/* Configure SSP0 pins*/
	// Configure all the pins of the LCD
	scu_pinmux(0x7,4,MD_PUP,FUNC0);	// P7.4 connected to GPIO = SPI_CSI
 	scu_pinmux(0xA,2,MD_PUP,FUNC0);	// PA.2 connected to GPIO = LCD_RST
 	scu_pinmux(0xA,3,MD_PUP,FUNC0);	// PA.3 connected to GPIO = LCD_A0
	scu_pinmux(0xF,3,(MD_PUP | MD_EHS),FUNC2);
	scu_pinmux(0xF,3,(MD_PUP | MD_EHS),FUNC2);

	//Set the directions
	GPIO_SetDir(0x3,(1<<12), 1);
	GPIO_SetDir(0x4,(1<<9), 1);
	GPIO_SetDir(0x4,(1<<10), 1);

   
	//Reset lcd
	GPIO_ClearValue(3,(1<<12));  //set CS
	GPIO_SetValue(4,(1<<9));
	delay(100);
	GPIO_ClearValue(4,(1<<9));
	delay(100);
	GPIO_SetValue(4,(1<<9));
	delay(100);
	GPIO_SetValue(3,(1<<12));  //set CS
	GPIO_SetValue(4,(1<<10));  //set A0
	delay(100);


	// initialize SSP configuration structure to default
	SSP_ConfigStructInit(&SSP_ConfigStruct);

	//Note that the LCD does not work as expected above 35-40Mb/s
	//Setting the speed to 10 Mb/s
	SSP_ConfigStruct.ClockRate = 10000000;
	
	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP0, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP0, ENABLE);
	delay(100);
	delay(100);
	xferConfig.length = 1;
	init_LCD(&xferConfig);

		    //wait for the lcd to reboot
	delay(100);
	delay(100);
	delay(100);
	delay(100);
	delay(100);

	xferConfig.tx_data = Tx_Buf;
	xferConfig.rx_data = Rx_Buf;

	comm_out(0xAE, &xferConfig);               //Display OFF
	comm_out(0x40, &xferConfig);               //Display start address + 0x40
	page = 0xB0;

	for(i=0;i<8;i++){            //32pixel display / 8 pixels per page = 4 pages
      comm_out(page, &xferConfig);            //send page address
      comm_out(0x10, &xferConfig);            //column address upper 4 bits + 0x10
      comm_out(0x00, &xferConfig);            //column address lower 4 bits + 0x00
      for(j=0;j<131;j++){         //128 columns wide
		 data_out(0x00, &xferConfig);
         }
      page++;                  //after 128 columns, go to next page
    }
    comm_out(0xAF, &xferConfig); 

	comm_out(0xAE, &xferConfig);               //Display OFF
	comm_out(0x40, &xferConfig);               //Display start address + 0x40
	comm_out(0xB0, &xferConfig); 
	comm_out(0x10, &xferConfig);            //column address upper 4 bits + 0x10
    comm_out(0x00, &xferConfig);            //column address lower 4 bits + 0x00
	comm_out(0xAF, &xferConfig);
	//delay(100);
	return &xferConfig;
}

#ifdef  DEBUG

void data_out(unsigned char i, SSP_DATA_SETUP_Type *xferConfig) //Data Output Serial Interface
{
   //unsigned int n;
   //CS = 0;               //Chip Select = Active
   GPIO_ClearValue(3,(1<<12));
   //A0 = 1 = Data
   //GPIO_SetValue(4,(1<<10));
   //delay(1);   

   Tx_Buf[0] = i;
   xferConfig->tx_data = Tx_Buf;
   SSP_ReadWrite(LPC_SSP0, xferConfig, SSP_TRANSFER_POLLING);

   //CS = 1;               //after 1 byte, Chip Select = inactive
   delay(1);
   GPIO_SetValue(3,(1<<12));
}

void comm_out(unsigned char j, SSP_DATA_SETUP_Type *xferConfig) //Command Output Serial Interface
{
   //unsigned int n;
   //CS = 0;			   //Chip Select = Active
   //GPIO_ClearValue(3,(1<<12));
   //A0 = 0;
   GPIO_ClearValue(4,(1<<10));
   //delay(1);
   
   Tx_Buf[0] = j;
   xferConfig->tx_data = Tx_Buf;
   SSP_ReadWrite(LPC_SSP0, xferConfig, SSP_TRANSFER_POLLING);

   //CS = 1;               //after 1 byte, Chip Select = inactive
   delay(1);								   
   GPIO_SetValue(3,(1<<12));
}

void init_LCD(SSP_DATA_SETUP_Type *xferConfig) 
{
	comm_out(0xA0, xferConfig);					 //RAM->SEG output = normal
	comm_out(0xAE, xferConfig);                  //Display OFF
	comm_out(0xC8, xferConfig);                  //COM scan direction = normal
	comm_out(0xA2, xferConfig);                  //1/9 bias
	
	comm_out(0x2F, xferConfig);                  //power control set
	comm_out(0x20, xferConfig);                  //resistor ratio set
	comm_out(0x81, xferConfig);                  //Electronic volume command (set contrast)
	comm_out(0x1F, xferConfig);                  //Electronic volume value (contrast value)
    comm_out(0xAF, xferConfig);
	delay(1);
	comm_out(0xAE, xferConfig);
	comm_out(0x21, xferConfig);
	comm_out(0xAF, xferConfig);

	delay(100);
}

void Init_Indexes() {
	 uint8_t i = 0, index = '!';
	 for(i = 1; i <= 93; i++, index++) {
	 	  Char_Index[index] = i;
	 }

	 Char_Index[' '] = 0;
}


void WriteChar(char ch, SSP_DATA_SETUP_Type *xferConfig, uint8_t nHighlight)
{
	uint8_t Col, i, index, temp;
	uint8_t lcd_Values[5], needRefresh = 0, Chars_Displayed_c = 0;
	UNS_16 raw_value[7], raw, DisplayStart,DisplayEnd, Page_No;

	comm_out(0xAE, xferConfig);

	//Update the array of characters that are displayed right now.
	if(ch == '\b' || ch == '\r') {
		comm_out(0xAF, xferConfig);
		return;
	}
	if(ch != '\n') {
	if(Number_of_Chars < MAX_CHARS_DISPLAYED) {
		Chars_Displayed[Current_Index] = ch;
		Current_Index++;
		Number_of_Chars++;
	} else {
		Start_Index++;
		if(Start_Index >= MAX_CHARS_DISPLAYED) {
			Current_Index = Start_Index - 1;
			Start_Index = 0;	
		} else {
			Current_Index = Start_Index - 1;
		}
		Chars_Displayed[Current_Index] = ch;
		Current_Index++;
	}

	if(nHighlight) {
		Highlight_Value[Current_Index-1] = Highlight;	
	} else {
		Highlight_Value[Current_Index-1] = NoHighlight;
	}
	}

	//Code to handle the next line character.
	else {
		index = (((Current_Index) + CHARS_PER_ROW) % CHARS_PER_ROW); //1 index
		index = CHARS_PER_ROW - index;
		ch = ' ';
		for(temp = 0; temp < index; temp++) {

			if(Number_of_Chars < MAX_CHARS_DISPLAYED) {
				Chars_Displayed[Current_Index] = ch;
				Current_Index++;
				Number_of_Chars++;
			} else {
				Start_Index++;
				if(Start_Index >= MAX_CHARS_DISPLAYED) {
					Current_Index = Start_Index - 1;
					Start_Index = 0;	
				} else {
					Current_Index = Start_Index - 1;
				}
				Chars_Displayed[Current_Index] = ch;
				Current_Index++;
			}
			//Chars_Displayed[i] = ' ';
			i++;
			for(Col = 0; Col < FONT_WIDTH; Col++)
        	data_out(0, xferConfig);
			//Start_Index++;
		}

		//if(Number_of_Chars >= MAX_CHARS_DISPLAYED) {
		if(!(Number_of_Chars < (MAX_CHARS_DISPLAYED + 1))) {
			//Send the scrollbar
			data_out(0xFF, xferConfig);
			data_out(0x81, xferConfig);
			data_out(0xFF, xferConfig);
		}
		//Current_Index = i;
		ch = '\n';
	}

	//Find if the whole screen needs to be updated.
	if(!(Number_of_Chars < (MAX_CHARS_DISPLAYED + 1))) {
		if(((Start_Index + CHARS_PER_ROW) % CHARS_PER_ROW) == 1) {
			needRefresh = 1;
		} else {
			needRefresh = 0;
		}
		DisplayStart = Start_Index + CHARS_PER_ROW;
		DisplayStart = DisplayStart - (DisplayStart % CHARS_PER_ROW);
		if(DisplayStart >= MAX_CHARS_DISPLAYED) {
			DisplayStart -= MAX_CHARS_DISPLAYED;
		}
	} else {
		 DisplayStart = Start_Index;
	}
	DisplayEnd = (Current_Index - 1);

	if(needRefresh == 0) {
		DisplayStart = DisplayEnd;
	}

	Page_No = ((Number_of_Chars-1) / CHARS_PER_ROW);
	Page_No |= 0xB0;

	if((Page_No == (0xB0 + NO_OF_ROWS)) && needRefresh == 1) {
		Page_No = 0xB0;
		comm_out(Page_No, xferConfig);            //send page address
    	comm_out(0x10, xferConfig);            //column address upper 4 bits + 0x10
    	comm_out(0x00, xferConfig);            //column address lower 4 bits + 0x00
	} else if ((((Number_of_Chars-1) % CHARS_PER_ROW) == 0) && (Page_No != 0xB4)) {
		comm_out(Page_No, xferConfig);            //send page address
    	comm_out(0x10, xferConfig);            //column address upper 4 bits + 0x10
    	comm_out(0x00, xferConfig);            //column address lower 4 bits + 0x00
		Page_No++;
	}

	if(Number_of_Chars == MAX_CHARS_DISPLAYED) {
		Number_of_Chars++;
	}

	if(ch == '\n') {
		comm_out(0xAF, xferConfig);
		return;
	}


	for(i = DisplayStart; i != (DisplayEnd+1); i++) {

	if(Chars_Displayed_c == CHARS_PER_ROW) {
		Page_No++;

		//Send the scrollbar
		if(needRefresh == 1){
			data_out(0xFF, xferConfig);
			data_out(0xFF, xferConfig);
			data_out(0xFF, xferConfig);
		}
		comm_out(Page_No, xferConfig);            //send page address
    	comm_out(0x10, xferConfig);            //column address upper 4 bits + 0x10
    	comm_out(0x00, xferConfig);            //column address lower 4 bits + 0x00
		Chars_Displayed_c = 0;
	}

	if(i == MAX_CHARS_DISPLAYED) {
		i = 0;
	}

	//Calculate the indexes that are to be sent.
	ch = Chars_Displayed[i];
	for(Col = 0; Col < 7; Col++) {
		raw_value[Col] = x5x7_bits[(ch * 7) + Col];
	}


	raw = 0;
	raw = ((raw_value[0] & (0x8000)) >> 15) | ((raw_value[1] & (0x8000)) >> 14)	| ((raw_value[2] & (0x8000)) >> 13)| ((raw_value[3] & (0x8000)) >> 12) | ((raw_value[4] & (0x8000)) >> 11) | ((raw_value[5] & (0x8000)) >> 10) |  ((raw_value[6] & (0x8000)) >> 9);
	lcd_Values[0] = raw;

	raw = 0;
	raw = ((raw_value[0] & (0x4000)) >> 14) | ((raw_value[1] & (0x4000)) >> 13)	| ((raw_value[2] & (0x4000)) >> 12)| ((raw_value[3] & (0x4000)) >> 11) | ((raw_value[4] & (0x4000)) >> 10) | ((raw_value[5] & (0x4000)) >> 9) |  ((raw_value[6] & (0x4000)) >> 8);
	lcd_Values[1] = raw;

	raw = 0;
	raw = ((raw_value[0] & (0x2000)) >> 13) | ((raw_value[1] & (0x2000)) >> 12)	| ((raw_value[2] & (0x2000)) >> 11)| ((raw_value[3] & (0x2000)) >> 10) | ((raw_value[4] & (0x2000)) >> 9) | ((raw_value[5] & (0x2000)) >> 8) |  ((raw_value[6] & (0x2000)) >>7);
	lcd_Values[2] = raw;

	raw = 0;
	raw = ((raw_value[0] & (0x1000)) >> 12) | ((raw_value[1] & (0x1000)) >> 11)	| ((raw_value[2] & (0x1000)) >> 10)| ((raw_value[3] & (0x1000)) >> 9) | ((raw_value[4] & (0x1000)) >> 8) | ((raw_value[5] & (0x1000)) >> 7) |  ((raw_value[6] & (0x1000)) >> 6);
	lcd_Values[3] = raw;

	lcd_Values[4] = 0;

    for(Col = 0; Col < FONT_WIDTH; Col++) {
        if(nHighlight || (Highlight_Value[i] == Highlight))
			data_out((~(lcd_Values[Col])) & 0x7F, xferConfig);
		else
			data_out(lcd_Values[Col], xferConfig);
	}

	Chars_Displayed_c++;
	}
    //data_out(0, xferConfig);

	if(needRefresh == 1) {
		//for(i = 0; i < CHARS_PER_ROW - 1; i++) {
		for(i = 0; i < CHARS_PER_ROW - 1; i++) {
			for(Col = 0; Col < FONT_WIDTH; Col++)
        	data_out(0, xferConfig);
		}

		//Send the scrollbar
		data_out(0xFF, xferConfig);
		data_out(0x81, xferConfig);
		data_out(0xFF, xferConfig);

		comm_out(0x10, xferConfig);            //column address upper 4 bits + 0x10
    	comm_out(0x05, xferConfig);            //column address lower 4 bits + 0x00
	}

	comm_out(0xAF, xferConfig);
}

#endif
#endif
/**
 * @}
 */
