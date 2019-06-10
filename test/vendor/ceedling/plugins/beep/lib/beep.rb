require 'ceedling/plugin'
require 'ceedling/constants'

class Beep < Plugin

  attr_reader :config

  def setup
    @config = {
      :on_done  => ((defined? TOOLS_BEEP_ON_DONE)  ? TOOLS_BEEP_ON_DONE  : :bell  ),
      :on_error => ((defined? TOOLS_BEEP_ON_ERROR) ? TOOLS_BEEP_ON_ERROR : :bell  ),
    }
  end

  def post_build
    beep @config[:on_done]
  end

  def post_error
    beep @config[:on_error]
  end

  private

  def beep(method = :none)
    case method
    when :bell
      if (SystemWrapper.windows?)
        puts "echo '\007'"
      else
        puts "echo -ne '\007'"
      end
    when :speaker_test
      `speaker-test -t sine -f 1000 -l 1`
    else
      #do nothing with illegal or :none
    end
  end
end

