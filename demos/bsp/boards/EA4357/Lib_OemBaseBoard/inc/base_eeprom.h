/*****************************************************************************
 *
 *   Copyright(C) 2011, Embedded Artists AB
 *   All rights reserved.
 *
 ******************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * Embedded Artists AB assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. Embedded Artists AB
 * reserves the right to make changes in the software without
 * notification. Embedded Artists AB also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 *****************************************************************************/
#ifndef __BASE_EEPROM_H
#define __BASE_EEPROM_H

#define BASE_EEPROM_TOTAL_SIZE 8192

void base_eeprom_init (void);
int16_t base_eeprom_read(uint8_t* buf, uint16_t offset, uint16_t len);
int16_t base_eeprom_write(uint8_t* buf, uint16_t offset, uint16_t len);


#endif /* end __BASE_EEPROM_H */
/****************************************************************************
**                            End Of File
*****************************************************************************/
