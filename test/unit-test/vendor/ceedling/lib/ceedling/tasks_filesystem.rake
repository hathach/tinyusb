
# rather than require 'rake/clean' & try to override, we replicate for finer control
CLEAN   = Rake::FileList["**/*~", "**/*.bak"]
CLOBBER = Rake::FileList.new

CLEAN.clear_exclude.exclude { |fn| fn.pathmap("%f") == 'core' && File.directory?(fn) }

CLEAN.include(File.join(PROJECT_TEST_BUILD_OUTPUT_PATH, '*'))
CLEAN.include(File.join(PROJECT_TEST_RESULTS_PATH, '*'))
CLEAN.include(File.join(PROJECT_TEST_DEPENDENCIES_PATH, '*'))
CLEAN.include(File.join(PROJECT_BUILD_RELEASE_ROOT, '*.*'))
CLEAN.include(File.join(PROJECT_RELEASE_BUILD_OUTPUT_PATH, '*'))
CLEAN.include(File.join(PROJECT_RELEASE_DEPENDENCIES_PATH, '*'))

CLOBBER.include(File.join(PROJECT_BUILD_ARTIFACTS_ROOT, '**/*'))
CLOBBER.include(File.join(PROJECT_BUILD_TESTS_ROOT, '**/*'))
CLOBBER.include(File.join(PROJECT_BUILD_RELEASE_ROOT, '**/*'))
CLOBBER.include(File.join(PROJECT_LOG_PATH, '**/*'))
CLOBBER.include(File.join(PROJECT_TEMP_PATH, '**/*'))

# just in case they're using git, let's make sure we allow them to preserved the build directory if desired.
CLOBBER.exclude(File.join(TESTS_BASE_PATH), '**/.gitkeep')

# because of cmock config, mock path can optionally exist apart from standard test build paths
CLOBBER.include(File.join(CMOCK_MOCK_PATH, '*'))

REMOVE_FILE_PROC = Proc.new { |fn| rm_r fn rescue nil }

# redefine clean so we can override how it advertises itself
desc "Delete all build artifacts and temporary products."
task(:clean) do
  # because :clean is a prerequisite for :clobber, intelligently display the progress message
  if (not @ceedling[:task_invoker].invoked?(/^clobber$/))
    @ceedling[:streaminator].stdout_puts("\nCleaning build artifacts...\n(For large projects, this task may take a long time to complete)\n\n")
  end
  begin
    CLEAN.each { |fn| REMOVE_FILE_PROC.call(fn) }
  rescue
  end
end

# redefine clobber so we can override how it advertises itself
desc "Delete all generated files (and build artifacts)."
task(:clobber => [:clean]) do
  @ceedling[:streaminator].stdout_puts("\nClobbering all generated files...\n(For large projects, this task may take a long time to complete)\n\n")
  begin
    CLOBBER.each { |fn| REMOVE_FILE_PROC.call(fn) }
    @ceedling[:rake_wrapper][:directories].invoke
    @ceedling[:dependinator].touch_force_rebuild_files
  rescue
  end
end

# create a directory task for each of the paths, so we know how to build them
PROJECT_BUILD_PATHS.each { |path| directory(path) }

# create a single directory task which verifies all the others get built
task :directories => PROJECT_BUILD_PATHS

# when the force file doesn't exist, it probably means we clobbered or are on a fresh
# install. In either case, stuff was deleted, so assume we want to rebuild it all
file @ceedling[:configurator].project_test_force_rebuild_filepath do
  unless File.exists?(@ceedling[:configurator].project_test_force_rebuild_filepath)
    @ceedling[:dependinator].touch_force_rebuild_files
  end
end

# list paths discovered at load time
namespace :paths do
  standard_paths = ['test','source','include']
  paths = @ceedling[:setupinator].config_hash[:paths].keys.map{|n| n.to_s.downcase}
  paths = (paths + standard_paths).uniq
  paths.each do |name|
    path_list = Object.const_get("COLLECTION_PATHS_#{name.upcase}")

    if (path_list.size != 0) || (standard_paths.include?(name))
      desc "List all collected #{name} paths."
      task(name.to_sym) { puts "#{name} paths:"; path_list.sort.each {|path| puts " - #{path}" } }
    end
  end

end


# list files & file counts discovered at load time
namespace :files do

  categories = [
    ['test',    COLLECTION_ALL_TESTS],
    ['source',  COLLECTION_ALL_SOURCE],
    ['include', COLLECTION_ALL_HEADERS],
    ['support', COLLECTION_ALL_SUPPORT]
  ]

  using_assembly = (defined?(TEST_BUILD_USE_ASSEMBLY) && TEST_BUILD_USE_ASSEMBLY) ||
                   (defined?(RELEASE_BUILD_USE_ASSEMBLY) && RELEASE_BUILD_USE_ASSEMBLY)
  categories << ['assembly', COLLECTION_ALL_ASSEMBLY] if using_assembly

  categories.each do |category|
    name       = category[0]
    collection = category[1]

    desc "List all collected #{name} files."
    task(name.to_sym) do
      puts "#{name} files:"
      collection.sort.each { |filepath| puts " - #{filepath}" }
      puts "file count: #{collection.size}"
    end
  end

end
