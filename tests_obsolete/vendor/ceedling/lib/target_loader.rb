module TargetLoader
  class NoTargets    < Exception; end
  class NoDirectory  < Exception; end
  class NoDefault    < Exception; end
  class NoSuchTarget < Exception; end

  class RequestReload < Exception; end

  def self.inspect(config, target_name=nil)
    unless config[:targets]
      raise NoTargets
    end

    targets = config[:targets]
    unless targets[:targets_directory]
      raise NoDirectory("No targets directory specified.")
    end
    unless targets[:default_target]
      raise NoDefault("No default target specified.")
    end

    target_path = lambda {|name| File.join(targets[:targets_directory], name + ".yml")}

    target = if target_name
               target_path.call(target_name)
             else
               target_path.call(targets[:default_target])
             end

    unless File.exists? target
      raise NoSuchTarget.new("No such target: #{target}")
    end

    ENV['CEEDLING_MAIN_PROJECT_FILE'] = target

    raise RequestReload
  end
end
