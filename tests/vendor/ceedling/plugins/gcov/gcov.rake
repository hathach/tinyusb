
directory(GCOV_BUILD_OUTPUT_PATH)
directory(GCOV_RESULTS_PATH)
directory(GCOV_ARTIFACTS_PATH)
directory(GCOV_DEPENDENCIES_PATH)

CLEAN.include(File.join(GCOV_BUILD_OUTPUT_PATH, '*'))
CLEAN.include(File.join(GCOV_RESULTS_PATH, '*'))
CLEAN.include(File.join(GCOV_DEPENDENCIES_PATH, '*'))

CLOBBER.include(File.join(GCOV_BUILD_PATH, '**/*'))


rule(/#{GCOV_BUILD_OUTPUT_PATH}\/#{'.+\\'+EXTENSION_OBJECT}$/ => [
    proc do |task_name|
      @ceedling[:file_finder].find_compilation_input_file(task_name)
    end
  ]) do |object|

  if (File.basename(object.source) =~ /^(#{PROJECT_TEST_FILE_PREFIX}|#{CMOCK_MOCK_PREFIX}|#{GCOV_IGNORE_SOURCES.join('|')})/i)
    @ceedling[:generator].generate_object_file(
      TOOLS_GCOV_COMPILER,
      GCOV_SYM,
      object.source,
      object.name,
      @ceedling[:file_path_utils].form_test_build_list_filepath( object.name ) )
  else
    @ceedling[GCOV_SYM].generate_coverage_object_file(object.source, object.name)
  end

end

rule(/#{GCOV_BUILD_OUTPUT_PATH}\/#{'.+\\'+EXTENSION_EXECUTABLE}$/) do |bin_file|
  @ceedling[:generator].generate_executable_file(
    TOOLS_GCOV_LINKER,
    GCOV_SYM,
    bin_file.prerequisites,
    bin_file.name,
    @ceedling[:file_path_utils].form_test_build_map_filepath(bin_file.name))
end

rule(/#{GCOV_RESULTS_PATH}\/#{'.+\\'+EXTENSION_TESTPASS}$/ => [
     proc do |task_name|
       @ceedling[:file_path_utils].form_test_executable_filepath(task_name)
     end
  ]) do |test_result|
  @ceedling[:generator].generate_test_results(TOOLS_GCOV_FIXTURE, GCOV_SYM, test_result.source, test_result.name)
end

rule(/#{GCOV_DEPENDENCIES_PATH}\/#{'.+\\'+EXTENSION_DEPENDENCIES}$/ => [
    proc do |task_name|
      @ceedling[:file_finder].find_compilation_input_file(task_name)
    end
  ]) do |dep|
  @ceedling[:generator].generate_dependencies_file(
    TOOLS_TEST_DEPENDENCIES_GENERATOR,
    GCOV_SYM,
    dep.source,
    File.join(GCOV_BUILD_OUTPUT_PATH, File.basename(dep.source).ext(EXTENSION_OBJECT) ),
    dep.name)
end

task :directories => [GCOV_BUILD_OUTPUT_PATH, GCOV_RESULTS_PATH, GCOV_DEPENDENCIES_PATH, GCOV_ARTIFACTS_PATH]

namespace GCOV_SYM do

  task :source_coverage => COLLECTION_ALL_SOURCE.pathmap("#{GCOV_BUILD_OUTPUT_PATH}/%n#{@ceedling[:configurator].extension_object}")

  desc "Run code coverage for all tests"
  task :all => [:directories] do
    @ceedling[:configurator].replace_flattened_config(@ceedling[GCOV_SYM].config)
    @ceedling[:test_invoker].setup_and_invoke(COLLECTION_ALL_TESTS, GCOV_SYM)
    @ceedling[:configurator].restore_config
  end

  desc "Run single test w/ coverage ([*] real test or source file name, no path)."
  task :* do
    message = "\nOops! '#{GCOV_ROOT_NAME}:*' isn't a real task. " +
              "Use a real test or source file name (no path) in place of the wildcard.\n" +
              "Example: rake #{GCOV_ROOT_NAME}:foo.c\n\n"
  
    @ceedling[:streaminator].stdout_puts( message )
  end
  
  desc "Run tests by matching regular expression pattern."
  task :pattern, [:regex] => [:directories] do |t, args|
    matches = []
    
    COLLECTION_ALL_TESTS.each do |test|
      matches << test if test =~ /#{args.regex}/
    end
  
    if (matches.size > 0)
      @ceedling[:configurator].replace_flattened_config(@ceedling[GCOV_SYM].config)
      @ceedling[:test_invoker].setup_and_invoke(matches, GCOV_SYM, {:force_run => false})
      @ceedling[:configurator].restore_config
    else
      @ceedling[:streaminator].stdout_puts("\nFound no tests matching pattern /#{args.regex}/.")
    end
  end

  desc "Run tests whose test path contains [dir] or [dir] substring."
  task :path, [:dir] => [:directories] do |t, args|
    matches = []
    
    COLLECTION_ALL_TESTS.each do |test|
      matches << test if File.dirname(test).include?(args.dir.gsub(/\\/, '/'))
    end
  
    if (matches.size > 0)
      @ceedling[:configurator].replace_flattened_config(@ceedling[GCOV_SYM].config)
      @ceedling[:test_invoker].setup_and_invoke(matches, GCOV_SYM, {:force_run => false})
      @ceedling[:configurator].restore_config
    else
      @ceedling[:streaminator].stdout_puts("\nFound no tests including the given path or path component.")
    end
  end

  desc "Run code coverage for changed files"
  task :delta => [:directories] do
    @ceedling[:configurator].replace_flattened_config(@ceedling[GCOV_SYM].config)
    @ceedling[:test_invoker].setup_and_invoke(COLLECTION_ALL_TESTS, GCOV_SYM, {:force_run => false})
    @ceedling[:configurator].restore_config
  end
  
  # use a rule to increase efficiency for large projects
  # gcov test tasks by regex
  rule(/^#{GCOV_TASK_ROOT}\S+$/ => [
      proc do |task_name|
        test = task_name.sub(/#{GCOV_TASK_ROOT}/, '')
        test = "#{PROJECT_TEST_FILE_PREFIX}#{test}" if not (test.start_with?(PROJECT_TEST_FILE_PREFIX))
        @ceedling[:file_finder].find_test_from_file_path(test)
      end
  ]) do |test|
    @ceedling[:rake_wrapper][:directories].invoke
    @ceedling[:configurator].replace_flattened_config(@ceedling[GCOV_SYM].config)
    @ceedling[:test_invoker].setup_and_invoke([test.source], GCOV_SYM)
    @ceedling[:configurator].restore_config
  end
  
end

if PROJECT_USE_DEEP_DEPENDENCIES
namespace REFRESH_SYM do
  task GCOV_SYM do
    @ceedling[:configurator].replace_flattened_config(@ceedling[GCOV_SYM].config)
    @ceedling[:test_invoker].refresh_deep_dependencies
    @ceedling[:configurator].restore_config
  end
end
end

