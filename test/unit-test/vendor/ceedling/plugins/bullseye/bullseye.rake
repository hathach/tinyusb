directory(BULLSEYE_BUILD_OUTPUT_PATH)
directory(BULLSEYE_RESULTS_PATH)
directory(BULLSEYE_ARTIFACTS_PATH)
directory(BULLSEYE_DEPENDENCIES_PATH)

CLEAN.include(File.join(BULLSEYE_BUILD_OUTPUT_PATH, '*'))
CLEAN.include(File.join(BULLSEYE_RESULTS_PATH, '*'))
CLEAN.include(File.join(BULLSEYE_DEPENDENCIES_PATH, '*'))

CLOBBER.include(File.join(BULLSEYE_BUILD_PATH, '**/*'))
PLUGINS_BULLSEYE_LIB_PATH = 'C:\\tools\\BullseyeCoverage\\lib' if not defined?(PLUGINS_BULLSEYE_LIB_PATH)

rule(/#{BULLSEYE_BUILD_OUTPUT_PATH}\/#{'.+\\'+EXTENSION_OBJECT}$/ => [
       proc do |task_name|
         @ceedling[:file_finder].find_compilation_input_file(task_name)
       end
     ]) do |object|

  if File.basename(object.source) =~ /^(#{PROJECT_TEST_FILE_PREFIX}|#{CMOCK_MOCK_PREFIX}|#{BULLSEYE_IGNORE_SOURCES.join('|')})/i
    @ceedling[:generator].generate_object_file(
      TOOLS_BULLSEYE_COMPILER,
      OPERATION_COMPILE_SYM,
      BULLSEYE_SYM,
      object.source,
      object.name,
      @ceedling[:file_path_utils].form_test_build_list_filepath(object.name)
    )
  else
    @ceedling[BULLSEYE_SYM].generate_coverage_object_file(object.source, object.name)
  end

end

rule(/#{BULLSEYE_BUILD_OUTPUT_PATH}\/#{'.+\\'+EXTENSION_EXECUTABLE}$/) do |bin_file|
  lib_args = @ceedling[:test_invoker].convert_libraries_to_arguments()
  lib_paths = @ceedling[:test_invoker].get_library_paths_to_arguments()
  @ceedling[:generator].generate_executable_file(
    TOOLS_BULLSEYE_LINKER,
    BULLSEYE_SYM,
    bin_file.prerequisites,
    bin_file.name,
    @ceedling[:file_path_utils].form_test_build_map_filepath(bin_file.name),
    lib_args,
    lib_paths
  )
end

rule(/#{BULLSEYE_RESULTS_PATH}\/#{'.+\\'+EXTENSION_TESTPASS}$/ => [
       proc do |task_name|
         @ceedling[:file_path_utils].form_test_executable_filepath(task_name)
       end
     ]) do |test_result|
  @ceedling[:generator].generate_test_results(TOOLS_BULLSEYE_FIXTURE, BULLSEYE_SYM, test_result.source, test_result.name)
end

rule(/#{BULLSEYE_DEPENDENCIES_PATH}\/#{'.+\\'+EXTENSION_DEPENDENCIES}$/ => [
       proc do |task_name|
         @ceedling[:file_finder].find_compilation_input_file(task_name)
       end
     ]) do |dep|
  @ceedling[:generator].generate_dependencies_file(
    TOOLS_TEST_DEPENDENCIES_GENERATOR,
    BULLSEYE_SYM,
    dep.source,
    File.join(BULLSEYE_BUILD_OUTPUT_PATH, File.basename(dep.source).ext(EXTENSION_OBJECT) ),
    dep.name
  )
end

task :directories => [BULLSEYE_BUILD_OUTPUT_PATH, BULLSEYE_RESULTS_PATH, BULLSEYE_DEPENDENCIES_PATH, BULLSEYE_ARTIFACTS_PATH]

namespace BULLSEYE_SYM do
  task source_coverage: COLLECTION_ALL_SOURCE.pathmap("#{BULLSEYE_BUILD_OUTPUT_PATH}/%n#{@ceedling[:configurator].extension_object}")

  desc 'Run code coverage for all tests'
  task all: [:test_deps] do
    @ceedling[:configurator].replace_flattened_config(@ceedling[BULLSEYE_SYM].config)
    @ceedling[BULLSEYE_SYM].enableBullseye(true)
    @ceedling[:test_invoker].setup_and_invoke(COLLECTION_ALL_TESTS, BULLSEYE_SYM)
    @ceedling[:configurator].restore_config
  end

  desc "Run single test w/ coverage ([*] real test or source file name, no path)."
  task :* do
    message = "\nOops! '#{BULLSEYE_ROOT_NAME}:*' isn't a real task. " +
              "Use a real test or source file name (no path) in place of the wildcard.\n" +
              "Example: rake #{BULLSEYE_ROOT_NAME}:foo.c\n\n"

    @ceedling[:streaminator].stdout_puts( message )
  end

  desc 'Run tests by matching regular expression pattern.'
  task :pattern, [:regex] => [:test_deps] do |_t, args|
    matches = []

    COLLECTION_ALL_TESTS.each do |test|
      matches << test if test =~ /#{args.regex}/
    end

    if !matches.empty?
      @ceedling[:configurator].replace_flattened_config(@ceedling[BULLSEYE_SYM].config)
      @ceedling[BULLSEYE_SYM].enableBullseye(true)
      @ceedling[:test_invoker].setup_and_invoke(matches, BULLSEYE_SYM, force_run: false)
      @ceedling[:configurator].restore_config
    else
      @ceedling[:streaminator].stdout_puts("\nFound no tests matching pattern /#{args.regex}/.")
    end
  end

  desc 'Run tests whose test path contains [dir] or [dir] substring.'
  task :path, [:dir] => [:test_deps] do |_t, args|
    matches = []

    COLLECTION_ALL_TESTS.each do |test|
      matches << test if File.dirname(test).include?(args.dir.tr('\\', '/'))
    end

    if !matches.empty?
      @ceedling[:configurator].replace_flattened_config(@ceedling[BULLSEYE_SYM].config)
      @ceedling[BULLSEYE_SYM].enableBullseye(true)
      @ceedling[:test_invoker].setup_and_invoke(matches, BULLSEYE_SYM, force_run: false)
      @ceedling[:configurator].restore_config
    else
      @ceedling[:streaminator].stdout_puts("\nFound no tests including the given path or path component.")
    end
  end

  desc 'Run code coverage for changed files'
  task delta: [:test_deps] do
    @ceedling[:configurator].replace_flattened_config(@ceedling[BULLSEYE_SYM].config)
    @ceedling[BULLSEYE_SYM].enableBullseye(true)
    @ceedling[:test_invoker].setup_and_invoke(COLLECTION_ALL_TESTS, BULLSEYE_SYM, {:force_run => false})
    @ceedling[:configurator].restore_config
  end

  # use a rule to increase efficiency for large projects
  # bullseye test tasks by regex
  rule(/^#{BULLSEYE_TASK_ROOT}\S+$/ => [
      proc do |task_name|
        test = task_name.sub(/#{BULLSEYE_TASK_ROOT}/, '')
        test = "#{PROJECT_TEST_FILE_PREFIX}#{test}" unless test.start_with?(PROJECT_TEST_FILE_PREFIX)
        @ceedling[:file_finder].find_test_from_file_path(test)
      end
  ]) do |test|
    @ceedling[:rake_wrapper][:test_deps].invoke
    @ceedling[:configurator].replace_flattened_config(@ceedling[BULLSEYE_SYM].config)
    @ceedling[BULLSEYE_SYM].enableBullseye(true)
    @ceedling[:test_invoker].setup_and_invoke([test.source], BULLSEYE_SYM)
    @ceedling[:configurator].restore_config
  end

end

if PROJECT_USE_DEEP_DEPENDENCIES
namespace REFRESH_SYM do
  task BULLSEYE_SYM do
    @ceedling[:configurator].replace_flattened_config(@ceedling[BULLSEYE_SYM].config)
    @ceedling[BULLSEYE_SYM].enableBullseye(true)
    @ceedling[:test_invoker].refresh_deep_dependencies
    @ceedling[:configurator].restore_config
  end
end
end

namespace UTILS_SYM do

  desc "Open Bullseye code coverage browser"
  task BULLSEYE_SYM do
    command = @ceedling[:tool_executor].build_command_line(TOOLS_BULLSEYE_BROWSER, [])
    @ceedling[:tool_executor].exec(command[:line], command[:options])
  end

end
