require 'rubygems'
require 'rake' # for ext() method
require 'ceedling/constants'


class GeneratorTestResultsSanityChecker

  constructor :configurator, :streaminator

  def verify(results, unity_exit_code)

    # do no sanity checking if it's disabled
    return if (@configurator.sanity_checks == TestResultsSanityChecks::NONE)
    raise "results nil or empty" if results.nil? || results.empty?

    ceedling_ignores_count   = results[:ignores].size
    ceedling_failures_count  = results[:failures].size
    ceedling_tests_summation = (ceedling_ignores_count + ceedling_failures_count + results[:successes].size)

    # Exit code handling is not a sanity check that can always be performed because
    # command line simulators may or may not pass through Unity's exit code
    if (@configurator.sanity_checks >= TestResultsSanityChecks::THOROUGH)
      # many platforms limit exit codes to a maximum of 255
      if ((ceedling_failures_count != unity_exit_code) and (unity_exit_code < 255))
        sanity_check_warning(results[:source][:file], "Unity's exit code (#{unity_exit_code}) does not match Ceedling's summation of failed test cases (#{ceedling_failures_count}).")
      end

      if ((ceedling_failures_count < 255) and (unity_exit_code == 255))
        sanity_check_warning(results[:source][:file], "Ceedling's summation of failed test cases (#{ceedling_failures_count}) is less than Unity's exit code (255 or more).")
      end
    end

    if (ceedling_ignores_count != results[:counts][:ignored])
      sanity_check_warning(results[:source][:file], "Unity's final ignore count (#{results[:counts][:ignored]}) does not match Ceedling's summation of ignored test cases (#{ceedling_ignores_count}).")
    end

    if (ceedling_failures_count != results[:counts][:failed])
      sanity_check_warning(results[:source][:file], "Unity's final fail count (#{results[:counts][:failed]}) does not match Ceedling's summation of failed test cases (#{ceedling_failures_count}).")
    end

    if (ceedling_tests_summation != results[:counts][:total])
      sanity_check_warning(results[:source][:file], "Unity's final test count (#{results[:counts][:total]}) does not match Ceedling's summation of all test cases (#{ceedling_tests_summation}).")
    end

  end

  private

  def sanity_check_warning(file, message)
    unless defined?(CEEDLING_IGNORE_SANITY_CHECK)
      notice = "\n" +
               "ERROR: Internal sanity check for test fixture '#{file.ext(@configurator.extension_executable)}' finds that #{message}\n" +
               "  Possible causes:\n" +
               "    1. Your test + source dereferenced a null pointer.\n" +
               "    2. Your test + source indexed past the end of a buffer.\n" +
               "    3. Your test + source committed a memory access violation.\n" +
               "    4. Your test fixture produced an exit code of 0 despite execution ending prematurely.\n" +
               "  Sanity check failures of test results are usually a symptom of interrupted test execution.\n\n"

      @streaminator.stderr_puts( notice )
      raise
    end
  end

end
