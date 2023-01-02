require 'ceedling/plugin'
require 'ceedling/streaminator'
require 'ceedling/constants'

class ColourReport < Plugin

  def setup
    @ceedling[:stream_wrapper].stdout_override(&ColourReport.method(:colour_stdout))
  end

  def self.colour_stdout(string)
    require 'colour_reporter.rb'
    report string
  end

end
