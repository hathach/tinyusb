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

// command, function, description
#define CLI_COMMAND_TABLE(ENTRY)   \
    ENTRY(unknow , cli_cmd_unknow, NULL) \
    ENTRY(help   , cli_cmd_help, NULL)   \

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
// IMPLEMENTATION
//--------------------------------------------------------------------+
tusb_error_t cli_cmd_unknow(char const * para)
{
  puts("unknown command, please type \"help\"");
  return TUSB_ERROR_NONE;
}

tusb_error_t cli_cmd_help(char const * para)
{
  puts("current supported commands are:");
  puts("cd\tchange directory");
  puts("ls\tlist directory");

  return TUSB_ERROR_NONE;
}


#define CLI_MAX_BUFFER   50
static char cli_buffer[CLI_MAX_BUFFER];

void cli_init(void)
{
  memclr_(cli_buffer, CLI_MAX_BUFFER);
}

void cli_poll(char ch)
{
  if ( isprint(ch) )
  {
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
  else if ( ch == '\r')
  {
    putchar('\n');
    for(cli_cmdtype_t cmd_id = CLI_CMDTYPE_help; cmd_id < CLI_CMDTYPE_COUNT; cmd_id++)
    {
      if( 0 == strncmp(cli_buffer, cli_string_tbl[cmd_id], CLI_MAX_BUFFER) )
      {
        cli_command_tbl[cmd_id](NULL);
        memclr_(cli_buffer, CLI_MAX_BUFFER);
        return;
      }
    }

    cli_cmd_unknow(NULL);
    memclr_(cli_buffer, CLI_MAX_BUFFER);
  }
  else if (ch=='\t') // \t may be used for auto-complete later
  {

  }
}

#endif
