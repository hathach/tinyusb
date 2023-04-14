require 'ceedling/plugin'
require 'ceedling/constants'
require 'json'

class JsonTestsReport < Plugin
  def setup
    @results_list = {}
    @test_counter = 0
  end

  def post_test_fixture_execute(arg_hash)
    context = arg_hash[:context]

    @results_list[context] = [] if @results_list[context].nil?

    @results_list[context] << arg_hash[:result_file]
  end

  def post_build
    @results_list.each_key do |context|
      results = @ceedling[:plugin_reportinator].assemble_test_results(@results_list[context])

      artifact_filename = @ceedling[:configurator].project_config_hash[:json_tests_report_artifact_filename] || 'report.json'
      artifact_fullpath = @ceedling[:configurator].project_config_hash[:json_tests_report_path] || File.join(PROJECT_BUILD_ARTIFACTS_ROOT, context.to_s)
      file_path = File.join(artifact_fullpath, artifact_filename)

      @ceedling[:file_wrapper].open(file_path, 'w') do |f|
        @test_counter = 1

        json = {
          "FailedTests" => write_failures(results[:failures]),
          "PassedTests" => write_tests(results[:successes]),
          "IgnoredTests" => write_tests(results[:ignores]),
          "Summary" => write_statistics(results[:counts])
        }

        f << JSON.pretty_generate(json)
      end
    end
  end

  private

  def write_failures(results)
    retval = []
    results.each do |result|
      result[:collection].each do |item|
        @test_counter += 1
        retval << {
          "file" => File.join(result[:source][:path], result[:source][:file]),
          "test" => item[:test],
          "line" => item[:line],
          "message" => item[:message]
        }
      end
    end
    return retval.uniq
  end

  def write_tests(results)
    retval = []
    results.each do |result|
      result[:collection].each do |item|
        @test_counter += 1
        retval << {
          "file" => File.join(result[:source][:path], result[:source][:file]),
          "test" => item[:test]
        }
      end
    end
    return retval
  end

  def write_statistics(counts)
    return {
      "total_tests" => counts[:total],
      "passed" => (counts[:total] - counts[:ignored] - counts[:failed]),
      "ignored" => counts[:ignored],
      "failures" => counts[:failed]
    }
  end

end
