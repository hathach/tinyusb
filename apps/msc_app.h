#ifndef __MSC_APP_H__
#define __MSC_APP_H__

/*
 * @brief read data from usb3 device
 *  true if read command is send successfully
 *  false  if operation is not successful
 */
bool usb_disk_read(void *buffer, uint32_t lba, uint16_t count);

/*
 * write data to usb3 device
 * @return
 *  true if read command is send successfully
 *  false  if operation is not successful
 */
bool usb_disk_write(void *buffer, uint32_t lba, uint16_t count);

/*
 * @brief  function to check whether the MSC mount process is completed or not
 * @return
 *      1 if MSC mount is completed
 *      0 otherwise
 */
int is_msc_mount_complete(void);

#endif /* __MSC_APP_H__ */
