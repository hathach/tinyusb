/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <ctype.h>
#include "tusb.h"
#include "bsp/board_api.h"

#include "ff.h"
#include "diskio.h"

// lib/embedded-cli
#define EMBEDDED_CLI_IMPL
#include "embedded_cli.h"

#include "msc_app.h"


//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

//------------- embedded-cli -------------//
#define CLI_BUFFER_SIZE     512
#define CLI_RX_BUFFER_SIZE  16
#define CLI_CMD_BUFFER_SIZE 64
#define CLI_HISTORY_SIZE    32
#define CLI_BINDING_COUNT   8

static EmbeddedCli *_cli;
static CLI_UINT cli_buffer[BYTES_TO_CLI_UINTS(CLI_BUFFER_SIZE)];

//------------- Elm Chan FatFS -------------//
static FATFS fatfs[CFG_TUH_DEVICE_MAX]; // for simplicity only support 1 LUN per device
static volatile bool _disk_busy[CFG_TUH_DEVICE_MAX];

// define the buffer to be place in USB/DMA memory with correct alignment/cache line size
CFG_TUH_MEM_SECTION static struct {
  TUH_EPBUF_TYPE_DEF(scsi_inquiry_resp_t, inquiry);
} scsi_resp;


//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool cli_init(void);

bool msc_app_init(void)
{
  for(size_t i=0; i<CFG_TUH_DEVICE_MAX; i++) {
    _disk_busy[i] = false;
  }

  // disable stdout buffered for echoing typing command
  #ifndef __ICCARM__ // TODO IAR doesn't support stream control ?
  setbuf(stdout, NULL);
  #endif

  cli_init();

  return true;
}

void msc_app_task(void)
{
  if (!_cli) {
    return;
  }

  int ch = board_getchar();
  if ( ch > 0 )
  {
    while( ch > 0 )
    {
      embeddedCliReceiveChar(_cli, (char) ch);
      ch = board_getchar();
    }
    embeddedCliProcess(_cli);
  }
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

static bool inquiry_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data) {
  msc_cbw_t const* cbw = cb_data->cbw;
  msc_csw_t const* csw = cb_data->csw;

  if (csw->status != 0)
  {
    printf("Inquiry failed\r\n");
    return false;
  }

  // Print out Vendor ID, Product ID and Rev
  printf("%.8s %.16s rev %.4s\r\n", scsi_resp.inquiry.vendor_id, scsi_resp.inquiry.product_id, scsi_resp.inquiry.product_rev);

  // Get capacity of device
  uint32_t const block_count = tuh_msc_get_block_count(dev_addr, cbw->lun);
  uint32_t const block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

  printf("Disk Size: %" PRIu32 " MB\r\n", block_count / ((1024*1024)/block_size));
  // printf("Block Count = %lu, Block Size: %lu\r\n", block_count, block_size);

  // For simplicity: we only mount 1 LUN per device
  uint8_t const drive_num = dev_addr-1;
  char drive_path[3] = "0:";
  drive_path[0] += drive_num;

  if ( f_mount(&fatfs[drive_num], drive_path, 1) != FR_OK )
  {
    puts("mount failed");
  }

  // change to newly mounted drive
  f_chdir(drive_path);

  // print the drive label
//  char label[34];
//  if ( FR_OK == f_getlabel(drive_path, label, NULL) )
//  {
//    puts(label);
//  }

  return true;
}

//------------- IMPLEMENTATION -------------//
void tuh_msc_mount_cb(uint8_t dev_addr)
{
  printf("A MassStorage device is mounted\r\n");

  uint8_t const lun = 0;
  tuh_msc_inquiry(dev_addr, lun, &scsi_resp.inquiry, inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
  printf("A MassStorage device is unmounted\r\n");

  uint8_t const drive_num = dev_addr-1;
  char drive_path[3] = "0:";
  drive_path[0] += drive_num;

  f_unmount(drive_path);

//  if ( phy_disk == f_get_current_drive() )
//  { // active drive is unplugged --> change to other drive
//    for(uint8_t i=0; i<CFG_TUH_DEVICE_MAX; i++)
//    {
//      if ( disk_is_ready(i) )
//      {
//        f_chdrive(i);
//        cli_init(); // refractor, rename
//      }
//    }
//  }
}

//--------------------------------------------------------------------+
// DiskIO
//--------------------------------------------------------------------+

static void wait_for_disk_io(BYTE pdrv)
{
  while(_disk_busy[pdrv])
  {
    tuh_task();
  }
}

static bool disk_io_complete(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data)
{
  (void) dev_addr; (void) cb_data;
  _disk_busy[dev_addr-1] = false;
  return true;
}

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
  uint8_t dev_addr = pdrv + 1;
  return tuh_msc_mounted(dev_addr) ? 0 : STA_NODISK;
}

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
  (void) pdrv;
	return 0; // nothing to do
}

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	uint8_t const dev_addr = pdrv + 1;
	uint8_t const lun = 0;

	_disk_busy[pdrv] = true;
	tuh_msc_read10(dev_addr, lun, buff, sector, (uint16_t) count, disk_io_complete, 0);
	wait_for_disk_io(pdrv);

	return RES_OK;
}

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	uint8_t const dev_addr = pdrv + 1;
	uint8_t const lun = 0;

	_disk_busy[pdrv] = true;
	tuh_msc_write10(dev_addr, lun, buff, sector, (uint16_t) count, disk_io_complete, 0);
	wait_for_disk_io(pdrv);

	return RES_OK;
}

#endif

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
  uint8_t const dev_addr = pdrv + 1;
  uint8_t const lun = 0;
  switch ( cmd )
  {
    case CTRL_SYNC:
      // nothing to do since we do blocking
      return RES_OK;

    case GET_SECTOR_COUNT:
      *((DWORD*) buff) = (WORD) tuh_msc_get_block_count(dev_addr, lun);
      return RES_OK;

    case GET_SECTOR_SIZE:
      *((WORD*) buff) = (WORD) tuh_msc_get_block_size(dev_addr, lun);
      return RES_OK;

    case GET_BLOCK_SIZE:
      *((DWORD*) buff) = 1;    // erase block size in units of sector size
      return RES_OK;

    default:
      return RES_PARERR;
  }

	return RES_OK;
}

//--------------------------------------------------------------------+
// CLI Commands
//--------------------------------------------------------------------+

void cli_cmd_cat(EmbeddedCli *cli, char *args, void *context);
void cli_cmd_cd(EmbeddedCli *cli, char *args, void *context);
void cli_cmd_cp(EmbeddedCli *cli, char *args, void *context);
void cli_cmd_ls(EmbeddedCli *cli, char *args, void *context);
void cli_cmd_pwd(EmbeddedCli *cli, char *args, void *context);
void cli_cmd_mkdir(EmbeddedCli *cli, char *args, void *context);
void cli_cmd_mv(EmbeddedCli *cli, char *args, void *context);
void cli_cmd_rm(EmbeddedCli *cli, char *args, void *context);

static void cli_write_char(EmbeddedCli *cli, char c) {
  (void) cli;
  putchar((int) c);
}

bool cli_init(void)
{
  EmbeddedCliConfig *config = embeddedCliDefaultConfig();
  config->cliBuffer         = cli_buffer;
  config->cliBufferSize     = CLI_BUFFER_SIZE;
  config->rxBufferSize      = CLI_RX_BUFFER_SIZE;
  config->cmdBufferSize     = CLI_CMD_BUFFER_SIZE;
  config->historyBufferSize = CLI_HISTORY_SIZE;
  config->maxBindingCount   = CLI_BINDING_COUNT;

  TU_ASSERT(embeddedCliRequiredSize(config) <= CLI_BUFFER_SIZE);

  _cli = embeddedCliNew(config);
  TU_ASSERT(_cli != NULL);

  _cli->writeChar = cli_write_char;

  embeddedCliAddBinding(_cli, (CliCommandBinding) {
    "cat",
    "Usage: cat [FILE]...\r\n\tConcatenate FILE(s) to standard output..",
    true,
    NULL,
    cli_cmd_cat
  });

  embeddedCliAddBinding(_cli, (CliCommandBinding) {
    "cd",
    "Usage: cd [DIR]...\r\n\tChange the current directory to DIR.",
    true,
    NULL,
    cli_cmd_cd
  });

  embeddedCliAddBinding(_cli, (CliCommandBinding) {
    "cp",
    "Usage: cp SOURCE DEST\r\n\tCopy SOURCE to DEST.",
    true,
    NULL,
    cli_cmd_cp
  });

  embeddedCliAddBinding(_cli, (CliCommandBinding) {
    "ls",
    "Usage: ls [DIR]...\r\n\tList information about the FILEs (the current directory by default).",
    true,
    NULL,
    cli_cmd_ls
  });

  embeddedCliAddBinding(_cli, (CliCommandBinding) {
    "pwd",
    "Usage: pwd\r\n\tPrint the name of the current working directory.",
    true,
    NULL,
    cli_cmd_pwd
  });

  embeddedCliAddBinding(_cli, (CliCommandBinding) {
    "mkdir",
    "Usage: mkdir DIR...\r\n\tCreate the DIRECTORY(ies), if they do not already exist..",
    true,
    NULL,
    cli_cmd_mkdir
  });

  embeddedCliAddBinding(_cli, (CliCommandBinding) {
    "mv",
    "Usage: mv SOURCE DEST...\r\n\tRename SOURCE to DEST.",
    true,
    NULL,
    cli_cmd_mv
  });

  embeddedCliAddBinding(_cli, (CliCommandBinding) {
    "rm",
    "Usage: rm [FILE]...\r\n\tRemove (unlink) the FILE(s).",
    true,
    NULL,
    cli_cmd_rm
  });

  return true;
}

void cli_cmd_cat(EmbeddedCli *cli, char *args, void *context)
{
  (void) cli; (void) context;

  uint16_t argc = embeddedCliGetTokenCount(args);

  // need at least 1 argument
  if ( argc == 0 )
  {
    printf("invalid arguments\r\n");
    return;
  }

  for(uint16_t i=0; i<argc; i++)
  {
    FIL fi;
    const char* fpath = embeddedCliGetToken(args, i+1); // token count from 1

    if ( FR_OK != f_open(&fi, fpath, FA_READ) )
    {
      printf("%s: No such file or directory\r\n", fpath);
    }else
    {
      uint8_t buf[512];
      UINT count = 0;
      while ( (FR_OK == f_read(&fi, buf, sizeof(buf), &count)) && (count > 0) )
      {
        for(UINT c = 0; c < count; c++)
        {
          const uint8_t ch = buf[c];
          if (isprint(ch) || iscntrl(ch))
          {
            putchar(ch);
          }else
          {
            putchar('.');
          }
        }
      }
    }

    f_close(&fi);
  }
}

void cli_cmd_cd(EmbeddedCli *cli, char *args, void *context)
{
  (void) cli; (void) context;

  uint16_t argc = embeddedCliGetTokenCount(args);

  // only support 1 argument
  if ( argc != 1 )
  {
    printf("invalid arguments\r\n");
    return;
  }

  // default is current directory
  const char* dpath = args;

  if ( FR_OK != f_chdir(dpath) )
  {
    printf("%s: No such file or directory\r\n", dpath);
    return;
  }
}

void cli_cmd_cp(EmbeddedCli *cli, char *args, void *context)
{
  (void) cli; (void) context;

  uint16_t argc = embeddedCliGetTokenCount(args);
  if ( argc != 2 )
  {
    printf("invalid arguments\r\n");
    return;
  }

  // default is current directory
  const char* src = embeddedCliGetToken(args, 1);
  const char* dst = embeddedCliGetToken(args, 2);

  FIL f_src;
  FIL f_dst;

  if ( FR_OK != f_open(&f_src, src, FA_READ) )
  {
    printf("cannot stat '%s': No such file or directory\r\n", src);
    return;
  }

  if ( FR_OK != f_open(&f_dst, dst, FA_WRITE | FA_CREATE_ALWAYS) )
  {
    printf("cannot create '%s'\r\n", dst);
    return;
  }else
  {
    uint8_t buf[512];
    UINT rd_count = 0;
    while ( (FR_OK == f_read(&f_src, buf, sizeof(buf), &rd_count)) && (rd_count > 0) )
    {
      UINT wr_count = 0;

      if ( FR_OK != f_write(&f_dst, buf, rd_count, &wr_count) )
      {
        printf("cannot write to '%s'\r\n", dst);
        break;
      }
    }
  }

  f_close(&f_src);
  f_close(&f_dst);
}

void cli_cmd_ls(EmbeddedCli *cli, char *args, void *context)
{
  (void) cli; (void) context;

  uint16_t argc = embeddedCliGetTokenCount(args);

  // only support 1 argument
  if ( argc > 1 )
  {
    printf("invalid arguments\r\n");
    return;
  }

  // default is current directory
  const char* dpath = ".";
  if (argc) dpath = args;

  DIR dir;
  if ( FR_OK != f_opendir(&dir, dpath) )
  {
    printf("cannot access '%s': No such file or directory\r\n", dpath);
    return;
  }

  FILINFO fno;
  while( (f_readdir(&dir, &fno) == FR_OK) && (fno.fname[0] != 0) )
  {
    if ( fno.fname[0] != '.' ) // ignore . and .. entry
    {
      if ( fno.fattrib & AM_DIR )
      {
        // directory
        printf("/%s\r\n", fno.fname);
      }else
      {
        printf("%-40s", fno.fname);
        if (fno.fsize < 1024)
        {
          printf("%" PRIu32 " B\r\n", fno.fsize);
        }else
        {
          printf("%" PRIu32 " KB\r\n", fno.fsize / 1024);
        }
      }
    }
  }

  f_closedir(&dir);
}

void cli_cmd_pwd(EmbeddedCli *cli, char *args, void *context)
{
  (void) cli; (void) context;
  uint16_t argc = embeddedCliGetTokenCount(args);

  if (argc != 0)
  {
    printf("invalid arguments\r\n");
    return;
  }

  char path[256];
  if (FR_OK != f_getcwd(path, sizeof(path)))
  {
    printf("cannot get current working directory\r\n");
  }

  puts(path);
}

void cli_cmd_mkdir(EmbeddedCli *cli, char *args, void *context)
{
  (void) cli; (void) context;

  uint16_t argc = embeddedCliGetTokenCount(args);

  // only support 1 argument
  if ( argc != 1 )
  {
    printf("invalid arguments\r\n");
    return;
  }

  // default is current directory
  const char* dpath = args;

  if ( FR_OK != f_mkdir(dpath) )
  {
    printf("%s: cannot create this directory\r\n", dpath);
    return;
  }
}

void cli_cmd_mv(EmbeddedCli *cli, char *args, void *context)
{
  (void) cli; (void) context;

  uint16_t argc = embeddedCliGetTokenCount(args);
  if ( argc != 2 )
  {
    printf("invalid arguments\r\n");
    return;
  }

  // default is current directory
  const char* src = embeddedCliGetToken(args, 1);
  const char* dst = embeddedCliGetToken(args, 2);

  if ( FR_OK != f_rename(src, dst) )
  {
    printf("cannot mv %s to %s\r\n", src, dst);
    return;
  }
}

void cli_cmd_rm(EmbeddedCli *cli, char *args, void *context)
{
  (void) cli; (void) context;

  uint16_t argc = embeddedCliGetTokenCount(args);

  // need at least 1 argument
  if ( argc == 0 )
  {
    printf("invalid arguments\r\n");
    return;
  }

  for(uint16_t i=0; i<argc; i++)
  {
    const char* fpath = embeddedCliGetToken(args, i+1); // token count from 1

    if ( FR_OK != f_unlink(fpath) )
    {
      printf("cannot remove '%s': No such file or directory\r\n", fpath);
    }
  }
}
