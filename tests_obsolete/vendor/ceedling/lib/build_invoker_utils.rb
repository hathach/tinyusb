require 'constants'


class BuildInvokerUtils

  constructor :configurator, :streaminator
  
  def process_exception(exception, context, test_build=true)
    if (exception.message =~ /Don't know how to build task '(.+)'/i)
      error_header  = "ERROR: Rake could not find file referenced in source"
      error_header += " or test" if (test_build) 
      error_header += ": '#{$1}'. Possible stale dependency."
      
      @streaminator.stderr_puts( error_header )

      if (@configurator.project_use_deep_dependencies)
        help_message = "Try fixing #include statements or adding missing file. Then run '#{REFRESH_TASK_ROOT}#{context.to_s}' task and try again."      
        @streaminator.stderr_puts( help_message )
      end
      
      raise ''
    else
      raise exception
    end
  end
  
end
