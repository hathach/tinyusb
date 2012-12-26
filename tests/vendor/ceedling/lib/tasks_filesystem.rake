
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
  CLEAN.each { |fn| REMOVE_FILE_PROC.call(fn) }
end

# redefine clobber so we can override how it advertises itself
desc "Delete all generated files (and build artifacts)."
task(:clobber => [:clean]) do
  @ceedling[:streaminator].stdout_puts("\nClobbering all generated files...\n(For large projects, this task may take a long time to complete)\n\n")
  CLOBBER.each { |fn| REMOVE_FILE_PROC.call(fn) }
end


PROJECT_BUILD_PATHS.each { |path| directory(path) }

# create directories that hold build output and generated files & touching rebuild dependency sources
task(:directories => PROJECT_BUILD_PATHS) { @ceedling[:dependinator].touch_force_rebuild_files }


# list paths discovered at load time
namespace :paths do
  
  paths = @ceedling[:setupinator].config_hash[:paths]
  paths.each_key do |section|
    name = section.to_s.downcase
    path_list = Object.const_get("COLLECTION_PATHS_#{name.upcase}")
    
    if (path_list.size != 0)
      desc "List all collected #{name} paths."
      task(name.to_sym) { puts "#{name} paths:"; path_list.sort.each {|path| puts " - #{path}" } }
    end
  end
  
end


# list files & file counts discovered at load time
namespace :files do
  
  categories = [
    ['test',   COLLECTION_ALL_TESTS],
    ['source', COLLECTION_ALL_SOURCE],
    ['header', COLLECTION_ALL_HEADERS]
    ]
  categories << ['assembly', COLLECTION_ALL_ASSEMBLY] if (RELEASE_BUILD_USE_ASSEMBLY)
  
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


