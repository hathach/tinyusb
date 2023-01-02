require 'ceedling/plugin'
require 'ceedling/defaults'

class StdoutIdeTestsReport < Plugin

  def setup
    @result_list = []
  end

  def post_test_fixture_execute(arg_hash)
    return if not (arg_hash[:context] == TEST_SYM)

    @result_list << arg_hash[:result_file]
  end

  def post_build
    return if (not @ceedling[:task_invoker].test_invoked?)

    results = @ceedling[:plugin_reportinator].assemble_test_results(@result_list)
    hash = {
      :header => '',
      :results => results
    }

    @ceedling[:plugin_reportinator].run_test_results_report(hash) do
      message = ''
      message = 'Unit test failures.' if (hash[:results][:counts][:failed] > 0)
      message
    end
  end

  def summary
    result_list = @ceedling[:file_path_utils].form_pass_results_filelist( PROJECT_TEST_RESULTS_PATH, COLLECTION_ALL_TESTS )

    # get test results for only those tests in our configuration and of those only tests with results on disk
    hash = {
      :header => '',
      :results => @ceedling[:plugin_reportinator].assemble_test_results(result_list, {:boom => false})
    }

    @ceedling[:plugin_reportinator].run_test_results_report(hash)
  end

end
