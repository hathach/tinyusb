require 'ceedling/constants'

class Streaminator

  constructor :streaminator_helper, :verbosinator, :loginator, :stream_wrapper

  # for those objects for whom the configurator has already been instantiated,
  # Streaminator is a convenience object for handling verbosity and writing to the std streams

  def stdout_puts(string, verbosity=Verbosity::NORMAL)
    if (@verbosinator.should_output?(verbosity))
      @stream_wrapper.stdout_puts(string)
      @stream_wrapper.stdout_flush
    end
    
    # write to log as though Verbosity::OBNOXIOUS
    @loginator.log( string, @streaminator_helper.extract_name($stdout) )
  end

  def stderr_puts(string, verbosity=Verbosity::NORMAL)
    if (@verbosinator.should_output?(verbosity))
      @stream_wrapper.stderr_puts(string)
      @stream_wrapper.stderr_flush
    end

    # write to log as though Verbosity::OBNOXIOUS
    @loginator.log( string, @streaminator_helper.extract_name($stderr) )
  end

  def stream_puts(stream, string, verbosity=Verbosity::NORMAL)
    if (@verbosinator.should_output?(verbosity))
      stream.puts(string)
      stream.flush
    end

    # write to log as though Verbosity::OBNOXIOUS
    @loginator.log( string, @streaminator_helper.extract_name(stream) )
  end

end
