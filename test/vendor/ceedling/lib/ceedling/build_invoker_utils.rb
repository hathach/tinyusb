require 'ceedling/constants'

##
# Utilities for raiser and reporting errors during building.
class BuildInvokerUtils

  constructor :configurator, :streaminator
 
  ##
  # Processes exceptions and tries to display a useful message for the user.
  #
  # ==== Attriboops...utes
  #
  # * _exception_:  The exception given by a rescue statement.
  # * _context_:    A symbol representing where in the build the exception
  # occurs. 
  # * _test_build_: A bool to signify if the exception occurred while building
  # from test or source.
  #
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
