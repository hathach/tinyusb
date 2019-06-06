require 'plugin'
require 'constants'

GCOV_ROOT_NAME         = 'gcov'
GCOV_TASK_ROOT         = GCOV_ROOT_NAME + ':'
GCOV_SYM               = GCOV_ROOT_NAME.to_sym

GCOV_BUILD_PATH        = "#{PROJECT_BUILD_ROOT}/#{GCOV_ROOT_NAME}"
GCOV_BUILD_OUTPUT_PATH = "#{GCOV_BUILD_PATH}/out"
GCOV_RESULTS_PATH      = "#{GCOV_BUILD_PATH}/results"
GCOV_DEPENDENCIES_PATH = "#{GCOV_BUILD_PATH}/dependencies"
GCOV_ARTIFACTS_PATH    = "#{PROJECT_BUILD_ARTIFACTS_ROOT}/#{GCOV_ROOT_NAME}"

GCOV_IGNORE_SOURCES    = ['unity', 'cmock', 'cexception']


class Gcov < Plugin

  attr_reader :config

  def setup
    @result_list = []  
  
    @config = {
      :project_test_build_output_path     => GCOV_BUILD_OUTPUT_PATH,
      :project_test_results_path          => GCOV_RESULTS_PATH,
      :project_test_dependencies_path     => GCOV_DEPENDENCIES_PATH,
      :defines_test                       => DEFINES_TEST + ['CODE_COVERAGE'],
      :collection_defines_test_and_vendor => COLLECTION_DEFINES_TEST_AND_VENDOR + ['CODE_COVERAGE']
      }
    
    @coverage_template_all = @ceedling[:file_wrapper].read( File.join( PLUGINS_GCOV_PATH, 'template.erb') )
  end

  def generate_coverage_object_file(source, object)
    compile_command = 
      @ceedling[:tool_executor].build_command_line(
        TOOLS_GCOV_COMPILER,
        source,
        object,
        @ceedling[:file_path_utils].form_test_build_list_filepath( object ) )
    @ceedling[:streaminator].stdout_puts("Compiling #{File.basename(source)} with coverage...")
    @ceedling[:tool_executor].exec( compile_command[:line], compile_command[:options] )
  end

  def post_test_fixture_execute(arg_hash)
    result_file = arg_hash[:result_file]
  
    if ((result_file =~ /#{GCOV_RESULTS_PATH}/) and (not @result_list.include?(result_file)))
      @result_list << arg_hash[:result_file]
    end
  end
    
  def post_build
    return if (not @ceedling[:task_invoker].invoked?(/^#{GCOV_TASK_ROOT}/))

    # test results
    results = @ceedling[:plugin_reportinator].assemble_test_results(@result_list)
    hash = {
      :header => GCOV_ROOT_NAME.upcase,
      :results => results
    }
    
    @ceedling[:plugin_reportinator].run_test_results_report(hash) do
      message = ''
      message = 'Unit test failures.' if (results[:counts][:failed] > 0)
      message
    end
    
    if (@ceedling[:task_invoker].invoked?(/^#{GCOV_TASK_ROOT}(all|delta)/))
      report_coverage_results_summary(@ceedling[:test_invoker].sources)
    else
      report_per_file_coverage_results(@ceedling[:test_invoker].sources)
    end
  end

  def summary
    result_list = @ceedling[:file_path_utils].form_pass_results_filelist( GCOV_RESULTS_PATH, COLLECTION_ALL_TESTS )

    # test results
    # get test results for only those tests in our configuration and of those only tests with results on disk
    hash = {
      :header => GCOV_ROOT_NAME.upcase,
      :results => @ceedling[:plugin_reportinator].assemble_test_results(result_list, {:boom => false})
    }

    @ceedling[:plugin_reportinator].run_test_results_report(hash)
    
    # coverage results
    # command = @ceedling[:tool_executor].build_command_line(TOOLS_GCOV_REPORT_COVSRC)
    # shell_result = @ceedling[:tool_executor].exec(command[:line], command[:options])
    # report_coverage_results_all(shell_result[:output])
  end

  private ###################################

  def report_coverage_results_summary(sources)

  end

  def report_per_file_coverage_results(sources)
    banner = @ceedling[:plugin_reportinator].generate_banner "#{GCOV_ROOT_NAME.upcase}: CODE COVERAGE SUMMARY"
    @ceedling[:streaminator].stdout_puts "\n" + banner

    coverage_sources = sources.clone
    coverage_sources.delete_if {|item| item =~ /#{CMOCK_MOCK_PREFIX}.+#{EXTENSION_SOURCE}$/}
    coverage_sources.delete_if {|item| item =~ /#{GCOV_IGNORE_SOURCES.join('|')}#{EXTENSION_SOURCE}$/}

    coverage_sources.each do |source|
      basename         = File.basename(source)
      command          = @ceedling[:tool_executor].build_command_line(TOOLS_GCOV_REPORT, basename)
      shell_results    = @ceedling[:tool_executor].exec(command[:line], command[:options])
      coverage_results = shell_results[:output]

      if (coverage_results.strip =~ /(File\s+'#{Regexp.escape(source)}'.+$)/m)
        report = ((($1.lines.to_a)[1..-1])).map{|line| basename + ' ' + line}.join('')
        @ceedling[:streaminator].stdout_puts(report + "\n\n")
      end
    end
  end

end

# end blocks always executed following rake run
END {
  # cache our input configurations to use in comparison upon next execution
  @ceedling[:cacheinator].cache_test_config( @ceedling[:setupinator].config_hash ) if (@ceedling[:task_invoker].invoked?(/^#{GCOV_TASK_ROOT}/))
}
