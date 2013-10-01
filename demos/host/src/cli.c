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

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define CLI_MAX_BUFFER        256
#define CLI_FILE_READ_BUFFER  (4*1024)

enum {
  ASCII_BACKSPACE = 8,
};

typedef enum {
  CLI_ERROR_NONE = 0,
  CLI_ERROR_INVALID_PARA,
  CLI_ERROR_INVALID_PATH,
  CLI_ERROR_FILE_EXISTED,
  CLI_ERROR_FAILED
}cli_error_t;

static char const * const cli_error_message[] =
{
  [CLI_ERROR_NONE         ] = 0,
  [CLI_ERROR_INVALID_PARA ] = "Invalid parameter(s)",
  [CLI_ERROR_INVALID_PATH ] = "No such file or directory",
  [CLI_ERROR_FILE_EXISTED ] = "file or directory already exists",
  [CLI_ERROR_FAILED       ] = "failed to execute"
};

//--------------------------------------------------------------------+
// CLI Database definition
//--------------------------------------------------------------------+

// command, function, description
#define CLI_COMMAND_TABLE(ENTRY)   \
    ENTRY(unknown , cli_cmd_unknown  , NULL                                                                  ) \
    ENTRY(help    , cli_cmd_help     , NULL                                                                  ) \
    ENTRY(cls     , cli_cmd_clear    , "Clear the screen."                                                   ) \
    ENTRY(ls      , cli_cmd_list     , "List information about the FILEs (the current directory by default).") \
    ENTRY(cd      , cli_cmd_changedir, "change the current directory."                                       ) \
    ENTRY(cat     , cli_cmd_cat      , "display contents of a file."                                         ) \
    ENTRY(cp      , cli_cmd_copy     , "Copies one or more files to another location."                       ) \
    ENTRY(mkdir   , cli_cmd_mkdir    , "Create a DIRECTORY, if it does not already exist."                   ) \

//--------------------------------------------------------------------+
// Expands the function to have the standard function signature
//--------------------------------------------------------------------+
#define CLI_PROTOTYPE_EXPAND(command, function, description) \
    cli_error_t function(char *);

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

typedef cli_error_t (* const cli_cmdfunc_t)(char *);
static cli_cmdfunc_t cli_command_tbl[] =
{
  CLI_COMMAND_TABLE(CMD_LOOKUP_EXPAND)
};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static char cli_buffer[CLI_MAX_BUFFER];
uint8_t fileread_buffer[CLI_FILE_READ_BUFFER] TUSB_CFG_ATTR_USBRAM;
static char volume_label[20];

static inline void drive_number2letter(char * p_path) ATTR_ALWAYS_INLINE;
static inline void drive_number2letter(char * p_path)
{
  if (p_path[1] == ':')
  {
    p_path[0] = 'E' + p_path[0] - '0' ;
  }
}

static inline void drive_letter2number(char * p_path) ATTR_ALWAYS_INLINE;
static inline void drive_letter2number(char * p_path)
{
  if (p_path[1] == ':')
  {
    p_path[0] = p_path[0] - 'E' + '0';
  }
}


//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
// NOTES: prompt re-use cli_buffer --> should not be called when cli_buffer has contents
void cli_command_prompt(void)
{
  f_getcwd(cli_buffer, CLI_MAX_BUFFER);
  drive_number2letter(cli_buffer);
  printf("\n%s %s\n$ ",
         (volume_label[0] !=0) ? volume_label : "No Label",
         cli_buffer);

  memclr_(cli_buffer, CLI_MAX_BUFFER);
}

void cli_init(void)
{
  memclr_(cli_buffer, CLI_MAX_BUFFER);
  f_getlabel(NULL, volume_label, NULL);
  cli_command_prompt();
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
    //------------- Separate Command & Parameter -------------//
    putchar('\n');
    char* p_space = strchr(cli_buffer, ' ');
    uint32_t command_len = (p_space == NULL) ? strlen(cli_buffer) : (p_space - cli_buffer);
    char* p_para = (p_space == NULL) ? (cli_buffer+command_len) : (p_space+1); // point to NULL-character or after space

    //------------- Find entered command in lookup table & execute it -------------//
    cli_cmdtype_t cmd_id;
    for(cmd_id = CLI_CMDTYPE_COUNT - 1; cmd_id > CLI_CMDTYPE_unknown; cmd_id--)
    {
      if( 0 == strncmp(cli_buffer, cli_string_tbl[cmd_id], command_len) )
      {
        break;
      }
    }

    cli_error_t error = cli_command_tbl[cmd_id]( p_para ); // command execution, (unknown command if cannot find)

    if (CLI_ERROR_NONE != error)  puts(cli_error_message[error]); // error message output if any

    //------------- print out current path -------------//
    cli_command_prompt();
  }
  else if (ch=='\t') // \t may be used for auto-complete later
  {

  }
}

//--------------------------------------------------------------------+
// UNKNOWN Command
//--------------------------------------------------------------------+
cli_error_t cli_cmd_unknown(char * para)
{
  puts("unknown command, please type \"help\" for list of supported commands");
  return CLI_ERROR_NONE;
}

//--------------------------------------------------------------------+
// HELP command
//--------------------------------------------------------------------+
cli_error_t cli_cmd_help(char * para)
{
  puts("current supported commands are:");
  for(cli_cmdtype_t cmd_id = CLI_CMDTYPE_help+1; cmd_id < CLI_CMDTYPE_COUNT; cmd_id++)
  {
    printf("%s\t%s\n", cli_string_tbl[cmd_id], cli_description_tbl[cmd_id]);
  }

  return CLI_ERROR_NONE;
}

//--------------------------------------------------------------------+
// Clear Screen Command
//--------------------------------------------------------------------+
cli_error_t cli_cmd_clear(char* p_para)
{
  printf(ANSI_ERASE_SCREEN(2) ANSI_CURSOR_POSITION(1,1) );
  return CLI_ERROR_NONE;
}

//--------------------------------------------------------------------+
// LS Command
//--------------------------------------------------------------------+
cli_error_t cli_cmd_list(char * p_para)
{
  if ( strlen(p_para) == 0 ) // list current directory
  {
    DIR target_dir;
    if ( FR_OK != f_opendir(&target_dir, ".") ) return CLI_ERROR_FAILED;

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
        TCHAR const * const p_name = (dir_entry.lfname[0] != 0) ? dir_entry.lfname : dir_entry.fname;
        if ( dir_entry.fattrib & AM_DIR ) // directory
        {
          printf("/%s", p_name);
        }else
        {
          printf("%-40s%d KB", p_name, dir_entry.fsize / 1000);
        }
        putchar('\n');
      }
    }
  }
  else
  {
    puts("ls only supports list current directory only, try to cd to that folder first");
    return CLI_ERROR_INVALID_PARA;
  }

  return CLI_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CD Command
//--------------------------------------------------------------------+
cli_error_t cli_cmd_changedir(char * p_para)
{
  if ( strlen(p_para) == 0 ) return CLI_ERROR_INVALID_PARA;

  drive_letter2number(p_para);

  if ( FR_OK != f_chdir(p_para) )
  {
    return CLI_ERROR_INVALID_PATH;
  }

  if ( p_para[1] == ':')
  { // path has drive letter --> change drive, update volume label
    f_chdrive(p_para[0] - '0');
    f_getlabel(NULL, volume_label, NULL);
  }

  return CLI_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CAT Command
//--------------------------------------------------------------------+
cli_error_t cli_cmd_cat(char *p_para)
{
  if ( strlen(p_para) == 0 ) return CLI_ERROR_INVALID_PARA;

  FIL file;

  switch( f_open(&file, p_para, FA_READ) )
  {
    case FR_OK:
    {
      uint32_t bytes_read = 0;

      if ( (FR_OK == f_read(&file, fileread_buffer, CLI_FILE_READ_BUFFER, &bytes_read)) && (bytes_read > 0) )
      {
        if ( file.fsize < 0x80000 ) // ~ 500KB
        {
          putchar('\n');
          do {
            for(uint32_t i=0; i<bytes_read; i++) putchar( fileread_buffer[i] );
          }while( (FR_OK == f_read(&file, fileread_buffer, CLI_FILE_READ_BUFFER, &bytes_read)) && (bytes_read > 0) );
        }else
        { // not display file contents if first character is not printable (high chance of binary file)
          printf("%s 's contents is too large\n", p_para);
        }
      }
      f_close(&file);
    }
    break;

    case FR_INVALID_NAME:
      return CLI_ERROR_INVALID_PATH;

    default :
      return CLI_ERROR_FAILED;
  }

  return CLI_ERROR_NONE;
}

//--------------------------------------------------------------------+
// Make Directory command
//--------------------------------------------------------------------+
cli_error_t cli_cmd_mkdir(char *p_para)
{
  if ( strlen(p_para) == 0 ) return CLI_ERROR_INVALID_PARA;

  return (f_mkdir(p_para) == FR_OK) ? CLI_ERROR_NONE : CLI_ERROR_FAILED;
}

//--------------------------------------------------------------------+
// COPY command
//--------------------------------------------------------------------+
cli_error_t cli_cmd_copy(char *p_para)
{
  char* p_space = strchr(p_para, ' ');
  if ( p_space == NULL ) return CLI_ERROR_INVALID_PARA;

  *p_space = 0; // replace space by NULL-character
  char* p_dest = p_space+1;

  if ( strlen(p_dest) == 0 ) return CLI_ERROR_INVALID_PARA;

  //------------- Check Existence of source & dest file -------------//
  cli_error_t error = CLI_ERROR_NONE;
  FIL src_file, dest_file;

  if ( FR_OK != f_open(&src_file , p_para, FA_READ) )  return CLI_ERROR_INVALID_PATH;
  switch ( f_open(&dest_file, p_dest, FA_WRITE | FA_CREATE_NEW) )
  {
    case FR_EXIST:
      error = CLI_ERROR_FILE_EXISTED;
    break;\

    case FR_OK:
      while(1)
      {
        uint32_t bytes_read = 0;
        uint32_t bytes_write = 0;
        FRESULT res;

        res = f_read(&src_file, fileread_buffer, CLI_FILE_READ_BUFFER, &bytes_read);     /* Read a chunk of src file */
        if ( (res != FR_OK) || (bytes_read == 0) ) break; /* error or eof */

        res = f_write(&dest_file, fileread_buffer, bytes_read, &bytes_write);               /* Write it to the dst file */
        if ( (res != FR_OK) || (bytes_write < bytes_read) ) break; /* error or disk full */
      }

      f_close(&dest_file);
    break;

    default:
      error = CLI_ERROR_FAILED;
    break;
  }

  f_close(&src_file);

  return error;
}
#endif
