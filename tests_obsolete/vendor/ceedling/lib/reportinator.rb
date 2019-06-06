
class Reportinator

  def generate_banner(message, width=nil)
    dash_count = ((width.nil?) ? message.strip.length : width)
    return "#{'-' * dash_count}\n#{message}\n#{'-' * dash_count}\n"
  end

end
