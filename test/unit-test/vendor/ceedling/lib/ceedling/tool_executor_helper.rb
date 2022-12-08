require 'ceedling/constants' # for Verbosity enumeration & $stderr redirect enumeration

##
# Helper functions for the tool executor
class ToolExecutorHelper

  constructor :streaminator, :system_utils, :system_wrapper

  ##
  # Returns the stderr redirection based on the config and logging.
  # ==== Attributes
  #
  # * _tool_config_:  A hash containing config information.
  # * _logging_:  A boolean representing if logging is enabled or not.
  #
  def stderr_redirection(tool_config, logging)
    # if there's no logging enabled, return :stderr_redirect unmodified
    return tool_config[:stderr_redirect] if (not logging)

    # if there is logging enabled but the redirect is a custom value (not enum), return the custom string
    return tool_config[:stderr_redirect] if (tool_config[:stderr_redirect].class == String)

    # if logging is enabled but there's no custom string, return the AUTO enumeration so $stderr goes into the log
    return StdErrRedirect::AUTO
  end


  ##
  # Returns the background execution prepend based on the config.
  # ==== Attributes
  #
  # * _tool_config_:  A hash containing config information.
  #
  def background_exec_cmdline_prepend(tool_config)
    return nil if (tool_config.nil? || tool_config[:background_exec].nil?)

    config_exec = tool_config[:background_exec]

    if ((config_exec == BackgroundExec::AUTO) and (@system_wrapper.windows?))
      return 'start'
    end

    if (config_exec == BackgroundExec::WIN)
      return 'start'
    end

    return nil
  end


  ##
  # Modifies an executables path based on platform.
  # ==== Attributes
  #
  # * _executable_:  The executable's path.
  #
  def osify_path_separators(executable)
    return executable.gsub(/\//, '\\') if (@system_wrapper.windows?)
    return executable
  end

  ##
  # Returns the stderr redirect append based on the config.
  # ==== Attributes
  #
  # * _tool_config_:  A hash containing config information.
  #
  def stderr_redirect_cmdline_append(tool_config)
    return nil if (tool_config.nil? || tool_config[:stderr_redirect].nil?)

    config_redirect = tool_config[:stderr_redirect]
    redirect        = StdErrRedirect::NONE

    if (config_redirect == StdErrRedirect::AUTO)
       if (@system_wrapper.windows?)
         redirect = StdErrRedirect::WIN
       elsif (@system_utils.tcsh_shell?)
         redirect = StdErrRedirect::TCSH
       else
         redirect = StdErrRedirect::UNIX
       end
    end

    case redirect
      # we may need more complicated processing after some learning with various environments
      when StdErrRedirect::NONE then nil
      when StdErrRedirect::WIN  then '2>&1'
      when StdErrRedirect::UNIX then '2>&1'
      when StdErrRedirect::TCSH then '|&'
      else redirect.to_s
    end
  end

  ##
  # Returns the background execution append based on the config.
  # ==== Attributes
  #
  # * _tool_config_:  A hash containing config information.
  #
  def background_exec_cmdline_append(tool_config)
    return nil if (tool_config.nil? || tool_config[:background_exec].nil?)

    config_exec = tool_config[:background_exec]

    # if :auto & windows, then we already prepended 'start' and should append nothing
    return nil if ((config_exec == BackgroundExec::AUTO) and (@system_wrapper.windows?))

    # if :auto & not windows, then we append standard '&'
    return '&' if ((config_exec == BackgroundExec::AUTO) and (not @system_wrapper.windows?))

    # if explicitly Unix, then append '&'
    return '&' if (config_exec == BackgroundExec::UNIX)

  # * _command_str_:  A hash containing config information.
    # all other cases, including :none, :win, & anything unrecognized, append nothing
    return nil
  end

  ##
  # Outputs success results if command succeeded and we have verbosity cranked up.
  # ==== Attributes
  #
  # * _command_str_:  The command ran.
  # * _shell_results_:  The outputs of the command including exit code and
  # output.
  # * _boom_:  A boolean representing if a non zero result is erroneous.
  #
  def print_happy_results(command_str, shell_result, boom=true)
    if ((shell_result[:exit_code] == 0) or ((shell_result[:exit_code] != 0) and not boom))
      output  = "> Shell executed command:\n"
      output += "'#{command_str}'\n"
      output += "> Produced output:\n"             if (not shell_result[:output].empty?)
      output += "#{shell_result[:output].strip}\n" if (not shell_result[:output].empty?)
      output += "> And exited with status: [#{shell_result[:exit_code]}].\n" if (shell_result[:exit_code] != 0)
      output += "\n"

      @streaminator.stdout_puts(output, Verbosity::OBNOXIOUS)
    end
  end

  ##
  # Outputs failures results if command failed and we have verbosity set to minimum error level.
  # ==== Attributes
  #
  # * _command_str_:  The command ran.
  # * _shell_results_:  The outputs of the command including exit code and
  # output.
  # * _boom_:  A boolean representing if a non zero result is erroneous.
  #
  def print_error_results(command_str, shell_result, boom=true)
    if ((shell_result[:exit_code] != 0) and boom)
      output  = "ERROR: Shell command failed.\n"
      output += "> Shell executed command:\n"
      output += "'#{command_str}'\n"
      output += "> Produced output:\n"             if (not shell_result[:output].empty?)
      output += "#{shell_result[:output].strip}\n" if (not shell_result[:output].empty?)
      output += "> And exited with status: [#{shell_result[:exit_code]}].\n" if (shell_result[:exit_code] != nil)
      output += "> And then likely crashed.\n"                               if (shell_result[:exit_code] == nil)
      output += "\n"

      @streaminator.stderr_puts(output, Verbosity::ERRORS)
    end
  end
end
