
class Verbosinator

  constructor :configurator

  def should_output?(level)
    return (level <= @configurator.project_verbosity)
  end

end
