require 'ceedling/plugin'
require 'ceedling/constants'

class JunitTestsReport < Plugin

  def setup
    @results_list = {}
    @test_counter = 0
    @time_result = []
  end

  def post_test_fixture_execute(arg_hash)
    context = arg_hash[:context]

    @results_list[context] = [] if (@results_list[context].nil?)

    @results_list[context] << arg_hash[:result_file]
    @time_result << arg_hash[:shell_result][:time]

  end

  def post_build
    @results_list.each_key do |context|
      results = @ceedling[:plugin_reportinator].assemble_test_results(@results_list[context])
      file_path = File.join( PROJECT_BUILD_ARTIFACTS_ROOT, context.to_s, 'report.xml' )

      @ceedling[:file_wrapper].open( file_path, 'w' ) do |f|
        @testsuite_counter = 0
        @testcase_counter = 0
        suites = reorganise_results( results )

        write_header( results, f )
        suites.each{|suite| write_suite( suite, f ) }
        write_footer( f )
      end
    end
  end

  private

  def write_header( results, stream )
    results[:counts][:time] = @time_result.reduce(0, :+)
    stream.puts '<?xml version="1.0" encoding="utf-8" ?>'
    stream.puts('<testsuites tests="%<total>d" failures="%<failed>d" skipped="%<ignored>d" time="%<time>f">' % results[:counts])
  end

  def write_footer( stream )
    stream.puts '</testsuites>'
  end

  def reorganise_results( results )
    # Reorganise the output by test suite instead of by result
    suites = Hash.new{ |h,k| h[k] = {collection: [], total: 0, success: 0, failed: 0, ignored: 0, stdout: []} }
    results[:successes].each do |result|
      source = result[:source]
      name = source[:file].sub(/\..{1,4}$/, "")
      suites[name][:collection] += result[:collection].map{|test| test.merge(result: :success)}
      suites[name][:total] += result[:collection].length
      suites[name][:success] += result[:collection].length
    end
    results[:failures].each do |result|
      source = result[:source]
      name = source[:file].sub(/\..{1,4}$/, "")
      suites[name][:collection] += result[:collection].map{|test| test.merge(result: :failed)}
      suites[name][:total] += result[:collection].length
      suites[name][:failed] += result[:collection].length
    end
    results[:ignores].each do |result|
      source = result[:source]
      name = source[:file].sub(/\..{1,4}$/, "")
      suites[name][:collection] += result[:collection].map{|test| test.merge(result: :ignored)}
      suites[name][:total] += result[:collection].length
      suites[name][:ignored] += result[:collection].length
    end
    results[:stdout].each do |result|
      source = result[:source]
      name = source[:file].sub(/\..{1,4}$/, "")
      suites[name][:stdout] += result[:collection]
    end
    suites.map{|name, data| data.merge(name: name) }
  end

  def write_suite( suite, stream )
    suite[:time] = @time_result.shift
    stream.puts('  <testsuite name="%<name>s" tests="%<total>d" failures="%<failed>d" skipped="%<ignored>d" time="%<time>f">' % suite)

    suite[:collection].each do |test|
      write_test( test, stream )
    end

    unless suite[:stdout].empty?
      stream.puts('    <system-out>')
      suite[:stdout].each{|line| stream.puts line }
      stream.puts('    </system-out>')
    end

    stream.puts('  </testsuite>')
  end

  def write_test( test, stream )
    case test[:result]
    when :success
      stream.puts('    <testcase name="%<test>s" />' % test)
    when :failed
      stream.puts('    <testcase name="%<test>s">' % test)
      if test[:message].empty?
        stream.puts('      <failure />')
      else
        stream.puts('      <failure message="%s" />' % test[:message])
      end
      stream.puts('    </testcase>')
    when :ignored
      stream.puts('    <testcase name="%<test>s">' % test)
      stream.puts('      <skipped />')
      stream.puts('    </testcase>')
    end
  end
end
