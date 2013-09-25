/**************************************************************************/
/*!
    @file     cli.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/

#include "cli.h"
#include "ctype.h"

#if TUSB_CFG_HOST_MSC

#include "ff.h"
#include "diskio.h"
#include "boards/ansi_escape.h"

// command, function, description
#define CLI_COMMAND_TABLE(ENTRY)   \
    ENTRY(unknow, cli_cmd_unknow   , NULL)                              \
    ENTRY(help  , cli_cmd_help     , NULL)                              \
    ENTRY(ls    , cli_cmd_list     , "list items in current directory") \
    ENTRY(cd    , cli_cmd_changedir, "change current directory")        \
    ENTRY(cat   , cli_cmd_cat      , "display contents of a text file") \

//--------------------------------------------------------------------+
// Expands the function to have the standard function signature
//--------------------------------------------------------------------+
#define CLI_PROTOTYPE_EXPAND(command, function, description) \
    tusb_error_t function(char const *);\

CLI_COMMAND_TABLE(CLI_PROTOTYPE_EXPAND);

//--------------------------------------------------------------------+
// Expand to enum value
//--------------------------------------------------------------------+
#define CLI_ENUM_EXPAND(command, function, description)    CLI_CMDTYPE_##command,
typedef enum {
  CLI_COMMAND_TABLE(CLI_ENUM_EXPAND)
  CLI_CMDTYPE_COUNT
}cli_cmdtype_t;

//--------------------------------------------------------------------+
// Expand to string table
//--------------------------------------------------------------------+
#define CLI_STRING_EXPAND(command, function, description)    #command,
char const* const cli_string_tbl[] =
{
  CLI_COMMAND_TABLE(CLI_STRING_EXPAND)
  0
};

//--------------------------------------------------------------------+
// Expand to Description table
//--------------------------------------------------------------------+
#define CLI_DESCRIPTION_EXPAND(command, function, description)    description,
char const* const cli_description_tbl[] =
{
  CLI_COMMAND_TABLE(CLI_DESCRIPTION_EXPAND)
  0
};


//--------------------------------------------------------------------+
// Expand to Command Lookup Table
//--------------------------------------------------------------------+
#define CMD_LOOKUP_EXPAND(command, function, description)\
  [CLI_CMDTYPE_##command] = function,\

typedef tusb_error_t (* const cli_cmdfunc_t)(char const *);
static cli_cmdfunc_t cli_command_tbl[] =
{
  CLI_COMMAND_TABLE(CMD_LOOKUP_EXPAND)
};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
#define CLI_MAX_BUFFER        256
#define CLI_FILE_READ_BUFFER  (4*1024)

enum {
  ASCII_BACKSPACE = 8,
};

static char cli_buffer[CLI_MAX_BUFFER];

uint8_t fileread_buffer[CLI_FILE_READ_BUFFER] TUSB_CFG_ATTR_USBRAM;


//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

void cli_init(void)
{
  memclr_(cli_buffer, CLI_MAX_BUFFER);
}

void cli_poll(char ch)
{
  if ( isprint(ch) )
  { // accumulate & echo
    if (strlen(cli_buffer) < CLI_MAX_BUFFER)
    {
      cli_buffer[ strlen(cli_buffer) ] = ch;
      putchar(ch);
    }else
    {
      puts("cli buffer overflows");
      memclr_(cli_buffer, CLI_MAX_BUFFER);
    }
  }
  else if ( ch == ASCII_BACKSPACE && strlen(cli_buffer))
  {
    printf(ANSI_CURSOR_BACKWARD(1) ANSI_ERASE_LINE(0) ); // move cursor back & clear to the end of line
    cli_buffer[ strlen(cli_buffer)-1 ] = 0;
  }
  else if ( ch == '\r')
  { // execute command
    putchar('\n');
    char* p_space = strchr(cli_buffer, ' ');
    uint32_t command_len = (p_space == NULL) ? strlen(cli_buffer) : (p_space - cli_buffer);
    char* p_para = (p_space == NULL) ? NULL : (p_space+1);

    cli_cmdtype_t cmd_id;
    for(cmd_id = CLI_CMDTYPE_COUNT - 1; cmd_id > 0; cmd_id--)
    {
      if( 0 == strncmp(cli_buffer, cli_string_tbl[cmd_id], command_len) )
      {
        break;
      }
    }

    cli_command_tbl[cmd_id]( p_para );

    f_getcwd(cli_buffer, CLI_MAX_BUFFER);
    printf("\nMSC %c%s\n$ ",
           'E'+cli_buffer[0]-'0',
           cli_buffer+1);
    memclr_(cli_buffer, CLI_MAX_BUFFER);
  }
  else if (ch=='\t') // \t may be used for auto-complete later
  {

  }
}

//--------------------------------------------------------------------+
// UNKNOWN Command
//--------------------------------------------------------------------+
tusb_error_t cli_cmd_unknow(char const * para)
{
  puts("unknown command, please type \"help\"");
  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// HELP command
//--------------------------------------------------------------------+
tusb_error_t cli_cmd_help(char const * para)
{
  puts("current supported commands are:");
  for(cli_cmdtype_t cmd_id = CLI_CMDTYPE_help+1; cmd_id < CLI_CMDTYPE_COUNT; cmd_id++)
  {
    printf("%s\t%s\n", cli_string_tbl[cmd_id], cli_description_tbl[cmd_id]);
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// LS Command
//--------------------------------------------------------------------+
tusb_error_t cli_cmd_list(const char * p_para)
{
  DIR target_dir;

  if ( (p_para == NULL) ||  (strlen(p_para) == 0) ) // list current directory
  {
    ASSERT_INT( FR_OK, f_opendir(&target_dir, "."), TUSB_ERROR_FAILED) ;

    TCHAR long_filename[_MAX_LFN];
    FILINFO dir_entry =
    {
        .lfname = long_filename,
        .lfsize = _MAX_LFN
    };
    while( (f_readdir(&target_dir, &dir_entry) == FR_OK)  && dir_entry.fname[0] != 0)
    {
      if ( dir_entry.fname[0] != '.' ) // ignore . and .. entry
      {
        printf("%s%c\n",
               (dir_entry.lfname[0] != 0) ? dir_entry.lfname : dir_entry.fname,
               dir_entry.fattrib & AM_DIR ? '/' : ' ');
      }
    }
  }
  else
  {
    puts("ls only supports list current directory only, try to cd to that folder first");
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CD Command
//--------------------------------------------------------------------+
tusb_error_t cli_cmd_changedir(const char * p_para)
{
  if ( (p_para == NULL) ||  (strlen(p_para) == 0) ) return TUSB_ERROR_INVALID_PARA;

  if ( FR_OK != f_chdir(p_para) )
  {
    printf("%s : No such file or directory\n", p_para);
    return TUSB_ERROR_INVALID_PARA;
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CAT Command
//--------------------------------------------------------------------+
tusb_error_t cli_cmd_cat(const char *p_para)
{
  if ( (p_para == NULL) ||  (strlen(p_para) == 0) ) return TUSB_ERROR_INVALID_PARA;

  FIL file;

  switch( f_open(&file, p_para, FA_READ) )
  {
    case FR_OK:
    {
      uint32_t bytes_read = 0;
      if ( (FR_OK == f_read(&file, fileread_buffer, CLI_FILE_READ_BUFFER, &bytes_read)) && (bytes_read > 0) )
      {
        if ( isprint( fileread_buffer[0] ) )
        {
          putchar('\n');
          for(uint32_t i=0; i<bytes_read; i++)
          {
            putchar( fileread_buffer[i] );
          }
        }else
        {
          printf("%s 's contents is not printable\n", p_para);
        }
      }
      f_close(&file);
    }
    break;

    case FR_INVALID_NAME:
      printf("%s : No such file or directory\n", p_para);
      return TUSB_ERROR_INVALID_PARA;
    break;

    default :
      printf("failed to open %s\n", p_para);
      return TUSB_ERROR_FAILED;
    break;
  }

  return TUSB_ERROR_NONE;
}

#endif
