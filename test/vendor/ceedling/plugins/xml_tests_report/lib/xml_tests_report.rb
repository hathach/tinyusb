require 'ceedling/plugin'
require 'ceedling/constants'

class XmlTestsReport < Plugin
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

      file_path = File.join(PROJECT_BUILD_ARTIFACTS_ROOT, context.to_s, 'report.xml')

      @ceedling[:file_wrapper].open(file_path, 'w') do |f|
        @test_counter = 1
        write_results(results, f)
      end
    end
  end

  private

  def write_results(results, stream)
    write_header(stream)
    write_failures(results[:failures], stream)
    write_tests(results[:successes], stream, 'SuccessfulTests')
    write_tests(results[:ignores], stream, 'IgnoredTests')
    write_statistics(results[:counts], stream)
    write_footer(stream)
  end

  def write_header(stream)
    stream.puts "<?xml version='1.0' encoding='utf-8' ?>"
    stream.puts '<TestRun>'
  end

  def write_failures(results, stream)
    if results.size.zero?
      stream.puts "\t<FailedTests/>"
      return
    end

    stream.puts "\t<FailedTests>"

    results.each do |result|
      result[:collection].each do |item|
        filename = File.join(result[:source][:path], result[:source][:file])

        stream.puts "\t\t<Test id=\"#{@test_counter}\">"
        stream.puts "\t\t\t<Name>#{filename}::#{item[:test]}</Name>"
        stream.puts "\t\t\t<FailureType>Assertion</FailureType>"
        stream.puts "\t\t\t<Location>"
        stream.puts "\t\t\t\t<File>#{filename}</File>"
        stream.puts "\t\t\t\t<Line>#{item[:line]}</Line>"
        stream.puts "\t\t\t</Location>"
        stream.puts "\t\t\t<Message>#{item[:message]}</Message>"
        stream.puts "\t\t</Test>"
        @test_counter += 1
      end
    end

    stream.puts "\t</FailedTests>"
  end

  def write_tests(results, stream, tag)
    if results.size.zero?
      stream.puts "\t<#{tag}/>"
      return
    end

    stream.puts "\t<#{tag}>"

    results.each do |result|
      result[:collection].each do |item|
        stream.puts "\t\t<Test id=\"#{@test_counter}\">"
        stream.puts "\t\t\t<Name>#{File.join(result[:source][:path], result[:source][:file])}::#{item[:test]}</Name>"
        stream.puts "\t\t</Test>"
        @test_counter += 1
      end
    end

    stream.puts "\t</#{tag}>"
  end

  def write_statistics(counts, stream)
    stream.puts "\t<Statistics>"
    stream.puts "\t\t<Tests>#{counts[:total]}</Tests>"
    stream.puts "\t\t<Ignores>#{counts[:ignored]}</Ignores>"
    stream.puts "\t\t<FailuresTotal>#{counts[:failed]}</FailuresTotal>"
    stream.puts "\t\t<Errors>0</Errors>"
    stream.puts "\t\t<Failures>#{counts[:failed]}</Failures>"
    stream.puts "\t</Statistics>"
  end

  def write_footer(stream)
    stream.puts '</TestRun>'
  end
end
