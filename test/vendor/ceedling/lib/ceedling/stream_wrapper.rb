
class StreamWrapper

  def stdout_puts(string)
    $stdout.puts(string)
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
