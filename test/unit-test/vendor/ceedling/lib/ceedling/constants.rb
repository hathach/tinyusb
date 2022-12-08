
class Verbosity
  SILENT      = 0  # as silent as possible (though there are some messages that must be spit out)
  ERRORS      = 1  # only errors
  COMPLAIN    = 2  # spit out errors and warnings/notices
  NORMAL      = 3  # errors, warnings/notices, standard status messages
  OBNOXIOUS   = 4  # all messages including extra verbose output (used for lite debugging / verification)
  DEBUG       = 5  # special extra verbose output for hardcore debugging
end


class TestResultsSanityChecks
  NONE      = 0  # no sanity checking of test results
  NORMAL    = 1  # perform non-problematic checks
  THOROUGH  = 2  # perform checks that require inside knowledge of system workings
end


class StdErrRedirect
  NONE = :none
  AUTO = :auto
  WIN  = :win
  UNIX = :unix
  TCSH = :tcsh
end


class BackgroundExec
  NONE = :none
  AUTO = :auto
  WIN  = :win
  UNIX = :unix
end

unless defined?(PROJECT_ROOT)
  PROJECT_ROOT = Dir.pwd()
end

GENERATED_DIR_PATH = [['vendor', 'ceedling'], 'src', "test", ['test', 'support'], 'build'].each{|p| File.join(*p)}

EXTENSION_WIN_EXE    = '.exe'
EXTENSION_NONWIN_EXE = '.out'


CEXCEPTION_ROOT_PATH = 'c_exception'
CEXCEPTION_LIB_PATH  = "#{CEXCEPTION_ROOT_PATH}/lib"
CEXCEPTION_C_FILE    = 'CException.c'
CEXCEPTION_H_FILE    = 'CException.h'

UNITY_ROOT_PATH        = 'unity'
UNITY_LIB_PATH         = "#{UNITY_ROOT_PATH}/src"
UNITY_C_FILE           = 'unity.c'
UNITY_H_FILE           = 'unity.h'
UNITY_INTERNALS_H_FILE = 'unity_internals.h'

CMOCK_ROOT_PATH = 'cmock'
CMOCK_LIB_PATH  = "#{CMOCK_ROOT_PATH}/src"
CMOCK_C_FILE    = 'cmock.c'
CMOCK_H_FILE    = 'cmock.h'


DEFAULT_CEEDLING_MAIN_PROJECT_FILE = 'project.yml' unless defined?(DEFAULT_CEEDLING_MAIN_PROJECT_FILE) # main project file
DEFAULT_CEEDLING_USER_PROJECT_FILE = 'user.yml'    unless defined?(DEFAULT_CEEDLING_USER_PROJECT_FILE) # supplemental user config file

INPUT_CONFIGURATION_CACHE_FILE     = 'input.yml'   unless defined?(INPUT_CONFIGURATION_CACHE_FILE)     # input configuration file dump
DEFINES_DEPENDENCY_CACHE_FILE      = 'defines_dependency.yml' unless defined?(DEFINES_DEPENDENCY_CACHE_FILE) # preprocessor definitions for files

TEST_ROOT_NAME    = 'test'                unless defined?(TEST_ROOT_NAME)
TEST_TASK_ROOT    = TEST_ROOT_NAME + ':'  unless defined?(TEST_TASK_ROOT)
TEST_SYM          = TEST_ROOT_NAME.to_sym unless defined?(TEST_SYM)

RELEASE_ROOT_NAME = 'release'                unless defined?(RELEASE_ROOT_NAME)
RELEASE_TASK_ROOT = RELEASE_ROOT_NAME + ':'  unless defined?(RELEASE_TASK_ROOT)
RELEASE_SYM       = RELEASE_ROOT_NAME.to_sym unless defined?(RELEASE_SYM)

REFRESH_ROOT_NAME = 'refresh'                unless defined?(REFRESH_ROOT_NAME)
REFRESH_TASK_ROOT = REFRESH_ROOT_NAME + ':'  unless defined?(REFRESH_TASK_ROOT)
REFRESH_SYM       = REFRESH_ROOT_NAME.to_sym unless defined?(REFRESH_SYM)

UTILS_ROOT_NAME   = 'utils'                unless defined?(UTILS_ROOT_NAME)
UTILS_TASK_ROOT   = UTILS_ROOT_NAME + ':'  unless defined?(UTILS_TASK_ROOT)
UTILS_SYM         = UTILS_ROOT_NAME.to_sym unless defined?(UTILS_SYM)

OPERATION_COMPILE_SYM  = :compile  unless defined?(OPERATION_COMPILE_SYM)
OPERATION_ASSEMBLE_SYM = :assemble unless defined?(OPERATION_ASSEMBLE_SYM)
OPERATION_LINK_SYM     = :link     unless defined?(OPERATION_LINK_SYM)


RUBY_STRING_REPLACEMENT_PATTERN = /#\{.+\}/
RUBY_EVAL_REPLACEMENT_PATTERN   = /^\{(.+)\}$/
TOOL_EXECUTOR_ARGUMENT_REPLACEMENT_PATTERN = /(\$\{(\d+)\})/
TEST_STDOUT_STATISTICS_PATTERN  = /\n-+\s*(\d+)\s+Tests\s+(\d+)\s+Failures\s+(\d+)\s+Ignored\s+(OK|FAIL)\s*/i

NULL_FILE_PATH = '/dev/null'

TESTS_BASE_PATH   = TEST_ROOT_NAME
RELEASE_BASE_PATH = RELEASE_ROOT_NAME

VENDORS_FILES = %w(unity UnityHelper cmock CException).freeze
