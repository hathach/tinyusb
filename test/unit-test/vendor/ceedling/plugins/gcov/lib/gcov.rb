require 'ceedling/plugin'
require 'ceedling/constants'
require 'gcov_constants'

class Gcov < Plugin
  attr_reader :config

  def setup
    @result_list = []

    @config = {
      project_test_build_output_path: GCOV_BUILD_OUTPUT_PATH,
      project_test_build_output_c_path: GCOV_BUILD_OUTPUT_PATH,
      project_test_results_path: GCOV_RESULTS_PATH,
      project_test_dependencies_path: GCOV_DEPENDENCIES_PATH,
      defines_test: DEFINES_TEST + ['CODE_COVERAGE'],
      gcov_html_report_filter: GCOV_FILTER_EXCLUDE
    }

    @plugin_root = File.expand_path(File.join(File.dirname(__FILE__), '..'))
    @coverage_template_all = @ceedling[:file_wrapper].read(File.join(@plugin_root, 'assets/template.erb'))
  end

  def generate_coverage_object_file(source, object)
    lib_args = @ceedling[:test_invoker].convert_libraries_to_arguments()
    compile_command =
      @ceedling[:tool_executor].build_command_line(
        TOOLS_GCOV_COMPILER,
        @ceedling[:flaginator].flag_down(OPERATION_COMPILE_SYM, GCOV_SYM, source),
        source,
        object,
        @ceedling[:file_path_utils].form_test_build_list_filepath(object),
        lib_args
      )
    @ceedling[:streaminator].stdout_puts("Compiling #{File.basename(source)} with coverage...")
    @ceedling[:tool_executor].exec(compile_command[:line], compile_command[:options])
  end

  def post_test_fixture_execute(arg_hash)
    result_file = arg_hash[:result_file]

    if (result_file =~ /#{GCOV_RESULTS_PATH}/) && !@result_list.include?(result_file)
      @result_list << arg_hash[:result_file]
    end
  end

  def post_build
    return unless @ceedling[:task_invoker].invoked?(/^#{GCOV_TASK_ROOT}/)

    # test results
    results = @ceedling[:plugin_reportinator].assemble_test_results(@result_list)
    hash = {
      header: GCOV_ROOT_NAME.upcase,
      results: results
    }

    @ceedling[:plugin_reportinator].run_test_results_report(hash) do
      message = ''
      message = 'Unit test failures.' if results[:counts][:failed] > 0
      message
    end

    report_per_file_coverage_results(@ceedling[:test_invoker].sources)
  end

  def summary
    result_list = @ceedling[:file_path_utils].form_pass_results_filelist(GCOV_RESULTS_PATH, COLLECTION_ALL_TESTS)

    # test results
    # get test results for only those tests in our configuration and of those only tests with results on disk
    hash = {
      header: GCOV_ROOT_NAME.upcase,
      results: @ceedling[:plugin_reportinator].assemble_test_results(result_list, boom: false)
    }

    @ceedling[:plugin_reportinator].run_test_results_report(hash)
  end

  private ###################################

  def report_per_file_coverage_results(sources)
    banner = @ceedling[:plugin_reportinator].generate_banner "#{GCOV_ROOT_NAME.upcase}: CODE COVERAGE SUMMARY"
    @ceedling[:streaminator].stdout_puts "\n" + banner

    coverage_sources = @ceedling[:project_config_manager].filter_internal_sources(sources)
    coverage_sources.each do |source|
      basename         = File.basename(source)
      command          = @ceedling[:tool_executor].build_command_line(TOOLS_GCOV_REPORT, [], [basename])
      shell_results    = @ceedling[:tool_executor].exec(command[:line], command[:options])
      coverage_results = shell_results[:output]

      if coverage_results.strip =~ /(File\s+'#{Regexp.escape(source)}'.+$)/m
        report = Regexp.last_match(1).lines.to_a[1..-1].map { |line| basename + ' ' + line }.join('')
        @ceedling[:streaminator].stdout_puts(report + "\n\n")
      end
    end

    ignore_path_list = @ceedling[:file_system_utils].collect_paths(@ceedling[:configurator].project_config_hash[:gcov_uncovered_ignore_list] || [])
    ignore_uncovered_list = @ceedling[:file_wrapper].instantiate_file_list
    ignore_path_list.each do |path|
      if File.exists?(path) and not File.directory?(path)
        ignore_uncovered_list.include(path)
      else
        ignore_uncovered_list.include(File.join(path, "*#{EXTENSION_SOURCE}"))
      end
    end

    found_uncovered = false
    COLLECTION_ALL_SOURCE.each do |source|
      unless coverage_sources.include?(source)
        v = Verbosity::DEBUG
        msg = "Could not find coverage results for " + source
        if ignore_uncovered_list.include?(source)
          msg += " [IGNORED]"
        else
          found_uncovered = true
          v = Verbosity::NORMAL
        end
        msg += "\n"
        @ceedling[:streaminator].stdout_puts(msg, v)
      end
    end
    if found_uncovered
      if @ceedling[:configurator].project_config_hash[:gcov_abort_on_uncovered]
        @ceedling[:streaminator].stderr_puts("There were files with no coverage results: aborting.\n")
        exit(-1)
      end
    end
  end
end

# end blocks always executed following rake run
END {
  # cache our input configurations to use in comparison upon next execution
  @ceedling[:cacheinator].cache_test_config(@ceedling[:setupinator].config_hash) if @ceedling[:task_invoker].invoked?(/^#{GCOV_TASK_ROOT}/)
}
