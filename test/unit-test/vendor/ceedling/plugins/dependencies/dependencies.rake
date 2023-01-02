
DEPENDENCIES_LIBRARIES.each do |deplib|

  # Look up the name of this dependency library
  deplib_name = @ceedling[DEPENDENCIES_SYM].get_name(deplib)

  # Make sure the required working directories exists
  # (don't worry about the subdirectories. That's the job of the dep's build tool)
  paths = @ceedling[DEPENDENCIES_SYM].get_working_paths(deplib)
  paths.each {|path| directory(path) }
  task :directories => paths

  all_deps = @ceedling[DEPENDENCIES_SYM].get_static_libraries_for_dependency(deplib) +
             @ceedling[DEPENDENCIES_SYM].get_dynamic_libraries_for_dependency(deplib) +
             @ceedling[DEPENDENCIES_SYM].get_include_directories_for_dependency(deplib) +
             @ceedling[DEPENDENCIES_SYM].get_source_files_for_dependency(deplib)

  # Add a rule for building the actual libraries from dependency list
  (@ceedling[DEPENDENCIES_SYM].get_static_libraries_for_dependency(deplib) +
   @ceedling[DEPENDENCIES_SYM].get_dynamic_libraries_for_dependency(deplib)
  ).each do |libpath|
    file libpath do |filetask|
      path = filetask.name

      # We double-check that it doesn't already exist, because this process sometimes
      # produces multiple files, but they may have already been flagged as invoked
      unless (File.exists?(path))

        # Set Environment Variables, Fetch, and Build
        @ceedling[DEPENDENCIES_SYM].set_env_if_required(path)
        @ceedling[DEPENDENCIES_SYM].fetch_if_required(path)
        @ceedling[DEPENDENCIES_SYM].build_if_required(path)
      end
    end
  end

  # Add a rule for building the source and includes from dependency list
  (@ceedling[DEPENDENCIES_SYM].get_include_directories_for_dependency(deplib) +
   @ceedling[DEPENDENCIES_SYM].get_source_files_for_dependency(deplib)
  ).each do |libpath|
    task libpath do |filetask|
      path = filetask.name

      unless (File.file?(path) || File.directory?(path))

        # Set Environment Variables, Fetch, and Build
        @ceedling[DEPENDENCIES_SYM].set_env_if_required(path)
        @ceedling[DEPENDENCIES_SYM].fetch_if_required(path)
        @ceedling[DEPENDENCIES_SYM].build_if_required(path)
      end
    end
  end

  # Give ourselves a way to trigger individual dependencies
  namespace DEPENDENCIES_SYM do
    namespace :deploy do
      # Add task to directly just build this dependency
      task(deplib_name => @ceedling[DEPENDENCIES_SYM].get_dynamic_libraries_for_dependency(deplib)) do |t,args|
        @ceedling[DEPENDENCIES_SYM].deploy_if_required(deplib_name)
      end
    end

    namespace :make do
      # Add task to directly just build this dependency
      task(deplib_name => all_deps)
    end

    namespace :clean do
      # Add task to directly clobber this dependency
      task(deplib_name) do
        @ceedling[DEPENDENCIES_SYM].clean_if_required(deplib_name)
      end
    end

    namespace :fetch do
      # Add task to directly clobber this dependency
      task(deplib_name) do
        @ceedling[DEPENDENCIES_SYM].fetch_if_required(deplib_name)
      end
    end
  end

  # Add source files to our list of things to build during release
  source_files = @ceedling[DEPENDENCIES_SYM].get_source_files_for_dependency(deplib)
  task PROJECT_RELEASE_BUILD_TARGET => source_files

  # Finally, add the static libraries to our RELEASE build dependency list
  static_libs = @ceedling[DEPENDENCIES_SYM].get_static_libraries_for_dependency(deplib)
  task RELEASE_SYM => static_libs

  # Add the dynamic libraries to our RELEASE task dependency list so that they will be copied automatically
  dynamic_libs = @ceedling[DEPENDENCIES_SYM].get_dynamic_libraries_for_dependency(deplib)
  task RELEASE_SYM => dynamic_libs

  # Add the include dirs / files to our list of dependencies for release
  headers = @ceedling[DEPENDENCIES_SYM].get_include_directories_for_dependency(deplib)
  task RELEASE_SYM => headers

  # Paths to Libraries need to be Added to the Lib Path List
  all_libs = static_libs + dynamic_libs
  PATHS_LIBRARIES ||= []
  all_libs.each {|lib| PATHS_LIBRARIES << File.dirname(lib) }
  PATHS_LIBRARIES.uniq!
  PATHS_LIBRARIES.reject!{|s| s.empty?}

  # Libraries Need to be Added to the Library List
  LIBRARIES_SYSTEM ||= []
  all_libs.each {|lib| LIBRARIES_SYSTEM << File.basename(lib,'.*').sub(/^lib/,'') }
  LIBRARIES_SYSTEM.uniq!
  LIBRARIES_SYSTEM.reject!{|s| s.empty?}
end

# Add any artifact:include or :source folders to our release & test includes paths so linking and mocking work.
@ceedling[DEPENDENCIES_SYM].add_headers_and_sources()

# Add tasks for building or cleaning ALL depencies
namespace DEPENDENCIES_SYM do
  desc "Deploy missing dependencies."
  task :deploy => DEPENDENCIES_LIBRARIES.map{|deplib| "#{DEPENDENCIES_SYM}:deploy:#{@ceedling[DEPENDENCIES_SYM].get_name(deplib)}"}

  desc "Build any missing dependencies."
  task :make => DEPENDENCIES_LIBRARIES.map{|deplib| "#{DEPENDENCIES_SYM}:make:#{@ceedling[DEPENDENCIES_SYM].get_name(deplib)}"}

  desc "Clean all dependencies."
  task :clean => DEPENDENCIES_LIBRARIES.map{|deplib| "#{DEPENDENCIES_SYM}:clean:#{@ceedling[DEPENDENCIES_SYM].get_name(deplib)}"}

  desc "Fetch all dependencies."
  task :fetch => DEPENDENCIES_LIBRARIES.map{|deplib| "#{DEPENDENCIES_SYM}:fetch:#{@ceedling[DEPENDENCIES_SYM].get_name(deplib)}"}
end

namespace :files do
  desc "List all collected dependency libraries."
  task :dependencies do
    puts "dependency files:"
    deps = []
    DEPENDENCIES_LIBRARIES.each do |deplib|
      deps << @ceedling[DEPENDENCIES_SYM].get_static_libraries_for_dependency(deplib)
      deps << @ceedling[DEPENDENCIES_SYM].get_dynamic_libraries_for_dependency(deplib)
    end
    deps.flatten!
    deps.sort.each {|dep| puts " - #{dep}"}
    puts "file count: #{deps.size}"
  end
end

# Make sure that we build dependencies before attempting to tackle any of the unit tests
Rake::Task[:test_deps].enhance ['dependencies:make']
