
class StreamWrapper

  def stdout_override(&fnc)
    @stdout_overide_fnc = fnc
  end

  def stdout_puts(string)
    if @stdout_overide_fnc
      @stdout_overide_fnc.call(string)
    else
      $stdout.puts(string)
    end
  end

  def stdout_flush
    $stdout.flush
  end

  def stderr_puts(string)
    $stderr.puts(string)
  end

  def stderr_flush
    $stderr.flush
  end

end
