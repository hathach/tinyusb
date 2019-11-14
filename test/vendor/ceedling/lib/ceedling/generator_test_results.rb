require 'rubygems'
require 'rake' # for .ext()
require 'ceedling/constants'

class GeneratorTestResults

  constructor :configurator, :generator_test_results_sanity_checker, :yaml_wrapper

  def process_and_write_results(unity_shell_result, results_file, test_file)
    output_file = results_file

    results = get_results_structure

    results[:source][:path] = File.dirname(test_file)
    results[:source][:file] = File.basename(test_file)
    results[:time] = unity_shell_result[:time] unless unity_shell_result[:time].nil?

    # process test statistics
    if (unity_shell_result[:output] =~ TEST_STDOUT_STATISTICS_PATTERN)
      results[:counts][:total] = $1.to_i
      results[:counts][:failed] = $2.to_i
      results[:counts][:ignored] = $3.to_i
      results[:counts][:passed] = (results[:counts][:total] - results[:counts][:failed] - results[:counts][:ignored])
    end

    # remove test statistics lines
    output_string = unity_shell_result[:output].sub(TEST_STDOUT_STATISTICS_PATTERN, '')

    output_string.lines do |line|
      # process unity output
      case line
      when /(:IGNORE)/
        elements = extract_line_elements(line, results[:source][:file])
        results[:ignores] << elements[0]
        results[:stdout] << elements[1] if (!elements[1].nil?)
      when /(:PASS$)/
        elements = extract_line_elements(line, results[:source][:file])
        results[:successes] << elements[0]
        results[:stdout] << elements[1] if (!elements[1].nil?)
      when /(:FAIL)/
        elements = extract_line_elements(line, results[:source][:file])
        results[:failures] << elements[0]
        results[:stdout] << elements[1] if (!elements[1].nil?)
      else # collect up all other
        results[:stdout] << line.chomp
      end
    end

    @generator_test_results_sanity_checker.verify(results, unity_shell_result[:exit_code])

    output_file = results_file.ext(@configurator.extension_testfail) if (results[:counts][:failed] > 0)

    @yaml_wrapper.dump(output_file, results)

    return { :result_file => output_file, :result => results }
  end

  private

  def get_results_structure
    return {
      :source    => {:path => '', :file => ''},
      :successes => [],
      :failures  => [],
      :ignores   => [],
      :counts    => {:total => 0, :passed => 0, :failed => 0, :ignored  => 0},
      :stdout    => [],
      :time      => 0.0
      }
  end

  def extract_line_elements(line, filename)
    # handle anything preceding filename in line as extra output to be collected
    stdout = nil
    stdout_regex = /(.+)#{Regexp.escape(filename)}.+/i

    if (line =~ stdout_regex)
      stdout = $1.clone
      line.sub!(/#{Regexp.escape(stdout)}/, '')
    end

    # collect up test results minus and extra output
    elements = (line.strip.split(':'))[1..-1]

    return {:test => elements[1], :line => elements[0].to_i, :message => (elements[3..-1].join(':')).strip}, stdout if elements.size >= 3
    return {:test => '???', :line => -1, :message => nil} #fallback safe option. TODO better handling
  end

end
