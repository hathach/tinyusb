require 'rubygems'
require 'rake'
require 'ceedling/makefile' # our replacement for rake's make-style dependency loader

include Rake::DSL if defined?(Rake::DSL)

class Rake::Task
  attr_reader :already_invoked
end

class RakeWrapper

  def initialize
    @makefile_loader = Rake::MakefileLoader.new # use our custom replacement noted above
  end

  def [](task)
    return Rake::Task[task]
  end

  def task_list
    return Rake::Task.tasks
  end

  def create_file_task(file_task, dependencies)
    file(file_task => dependencies)
  end

  def load_dependencies(dependencies_path)
    @makefile_loader.load(dependencies_path)
  end

end
