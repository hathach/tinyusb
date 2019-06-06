require 'constants'

class ShellExecutionException < RuntimeError
  attr_reader :shell_result
  def initialize(shell_result)
    @shell_result = shell_result
  end
end

class ToolExecutor

  constructor :configurator, :tool_executor_helper, :streaminator, :system_wrapper

  def setup
    @tool_name  = ''
    @executable = ''
  end

  # build up a command line from yaml provided config
  def build_command_line(tool_config, *args)
    @tool_name  = tool_config[:name]
    @executable = tool_config[:executable]

    command = {}

    # basic premise is to iterate top to bottom through arguments using '$' as 
    #  a string replacement indicator to expand globals or inline yaml arrays
    #  into command line arguments via substitution strings
    command[:line] = [
      @tool_executor_helper.osify_path_separators( expandify_element(@executable, *args) ),
      build_arguments(tool_config[:arguments], *args),
      ].join(' ').strip

    command[:options] = {
      :stderr_redirect => @tool_executor_helper.stderr_redirection(tool_config, @configurator.project_logging),
      :background_exec => tool_config[:background_exec]
      }
    
    return command
  end


  # shell out, execute command, and return response
  def exec(command, options={}, args=[])
    options[:boom] = true if (options[:boom].nil?)
    options[:stderr_redirect] = StdErrRedirect::NONE if (options[:stderr_redirect].nil?)
    options[:background_exec] = BackgroundExec::NONE if (options[:background_exec].nil?)

    # build command line
    command_line = [
      @tool_executor_helper.background_exec_cmdline_prepend( options ),
      command.strip,
      args,
      @tool_executor_helper.stderr_redirect_cmdline_append( options ),
      @tool_executor_helper.background_exec_cmdline_append( options ),
      ].flatten.compact.join(' ')

    shell_result = {}
    
    # depending on background exec option, we shell out differently
    if (options[:background_exec] != BackgroundExec::NONE)
      shell_result = @system_wrapper.shell_system( command_line )
    else
      shell_result = @system_wrapper.shell_backticks( command_line )
    end
    
    @tool_executor_helper.print_happy_results( command_line, shell_result, options[:boom] )
    @tool_executor_helper.print_error_results( command_line, shell_result, options[:boom] )
    
    # go boom if exit code isn't 0 (but in some cases we don't want a non-0 exit code to raise)
    raise ShellExecutionException.new(shell_result) if ((shell_result[:exit_code] != 0) and options[:boom])
    
    return shell_result
  end

  
  private #############################

  
  def build_arguments(config, *args)
    build_string = ''
    
    return nil if (config.nil?)
    
    # iterate through each argument

    # the yaml blob array needs to be flattened so that yaml substitution
    # is handled correctly, since it creates a nested array when an anchor is
    # dereferenced
    config.flatten.each do |element|
      argument = ''
      
      case(element)
        # if we find a simple string then look for string replacement operators
        #  and expand with the parameters in this method's argument list
        when String then argument = expandify_element(element, *args)
        # if we find a hash, then we grab the key as a substitution string and expand the
        #  hash's value(s) within that substitution string
        when Hash   then argument = dehashify_argument_elements(element)
      end

      build_string.concat("#{argument} ") if (argument.length > 0)
    end
    
    build_string.strip!
    return build_string if (build_string.length > 0)
    return nil
  end


  # handle simple text string argument & argument array string replacement operators
  def expandify_element(element, *args)
    match = //
    to_process = nil
    args_index = 0

    # handle ${#} input replacement
    if (element =~ TOOL_EXECUTOR_ARGUMENT_REPLACEMENT_PATTERN)
      args_index = ($2.to_i - 1)

      if (args.nil? or args[args_index].nil?)
        @streaminator.stderr_puts("ERROR: Tool '#{@tool_name}' expected valid argument data to accompany replacement operator #{$1}.", Verbosity::ERRORS)
        raise
      end

      match = /#{Regexp.escape($1)}/
      to_process = args[args_index]
    end
      
    # simple string argument: replace escaped '\$' and strip
    element.sub!(/\\\$/, '$')
    element.strip!

    # handle inline ruby execution
    if (element =~ RUBY_EVAL_REPLACEMENT_PATTERN)
      element.replace(eval($1))
    end

    build_string = ''

    # handle array or anything else passed into method to be expanded in place of replacement operators
    case (to_process)
      when Array then to_process.each {|value| build_string.concat( "#{element.sub(match, value.to_s)} " ) } if (to_process.size > 0)
      else build_string.concat( element.sub(match, to_process.to_s) )
    end

    # handle inline ruby string substitution
    if (build_string =~ RUBY_STRING_REPLACEMENT_PATTERN)
      build_string.replace(@system_wrapper.module_eval(build_string))
    end
    
    return build_string.strip
  end

  
  # handle argument hash: keys are substitution strings, values are data to be expanded within substitution strings
  def dehashify_argument_elements(hash)
    build_string = ''
    elements = []

    # grab the substitution string (hash key)
    substitution = hash.keys[0].to_s
    # grab the string(s) to squirt into the substitution string (hash value)
    expand = hash[hash.keys[0]]

    if (expand.nil?)
      @streaminator.stderr_puts("ERROR: Tool '#{@tool_name}' could not expand nil elements for substitution string '#{substitution}'.", Verbosity::ERRORS)
      raise
    end
    
    # array-ify expansion input if only a single string
    expansion = ((expand.class == String) ? [expand] : expand)
    
    expansion.each do |item|
      # code eval substitution
      if (item =~ RUBY_EVAL_REPLACEMENT_PATTERN)
        elements << eval($1)
      # string eval substitution
      elsif (item =~ RUBY_STRING_REPLACEMENT_PATTERN)
        elements << @system_wrapper.module_eval(item)
      # global constants
      elsif (@system_wrapper.constants_include?(item))
        const = Object.const_get(item)
        if (const.nil?)
          @streaminator.stderr_puts("ERROR: Tool '#{@tool_name}' found constant '#{item}' to be nil.", Verbosity::ERRORS)
          raise
        else
          elements << const
        end
      elsif (item.class == Array)
        elements << item
      elsif (item.class == String)
        @streaminator.stderr_puts("ERROR: Tool '#{@tool_name}' cannot expand nonexistent value '#{item}' for substitution string '#{substitution}'.", Verbosity::ERRORS)
        raise        
      else
        @streaminator.stderr_puts("ERROR: Tool '#{@tool_name}' cannot expand value having type '#{item.class}' for substitution string '#{substitution}'.", Verbosity::ERRORS)
        raise        
      end
    end
    
    # expand elements (whether string or array) into substitution string & replace escaped '\$'
    elements.flatten!
    elements.each do |element|
      build_string.concat( substitution.sub(/([^\\]*)\$/, "\\1#{element}") ) # don't replace escaped '\$' but allow us to replace just a lonesome '$'
      build_string.gsub!(/\\\$/, '$')
      build_string.concat(' ')
    end

    return build_string.strip
  end

end
