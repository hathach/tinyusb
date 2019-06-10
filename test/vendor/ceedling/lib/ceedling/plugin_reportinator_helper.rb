require 'erb'
require 'rubygems'
require 'rake' # for ext()
require 'ceedling/constants'

class PluginReportinatorHelper
  
  attr_writer :ceedling
  
  constructor :configurator, :streaminator, :yaml_wrapper, :file_wrapper
  
  def fetch_results(results_path, options)
    pass_path = File.join(results_path.ext( @configurator.extension_testpass ))
    fail_path = File.join(results_path.ext( @configurator.extension_testfail ))

    if (@file_wrapper.exist?(fail_path))
      return @yaml_wrapper.load(fail_path)
    elsif (@file_wrapper.exist?(pass_path))
      return @yaml_wrapper.load(pass_path)
    else
      if (options[:boom])
        @streaminator.stderr_puts("Could find no test results for '#{File.basename(results_path).ext(@configurator.extension_source)}'", Verbosity::ERRORS)
        raise
      end
    end
    
    return {}
  end


  def process_results(aggregate_results, results)
    return if (results.empty?)
    aggregate_results[:successes]        << { :source => results[:source].clone, :collection => results[:successes].clone } if (results[:successes].size > 0)
    aggregate_results[:failures]         << { :source => results[:source].clone, :collection => results[:failures].clone  } if (results[:failures].size > 0)
    aggregate_results[:ignores]          << { :source => results[:source].clone, :collection => results[:ignores].clone   } if (results[:ignores].size > 0)
    aggregate_results[:stdout]           << { :source => results[:source].clone, :collection => results[:stdout].clone    } if (results[:stdout].size > 0)
    aggregate_results[:counts][:total]   += results[:counts][:total]
    aggregate_results[:counts][:passed]  += results[:counts][:passed]
    aggregate_results[:counts][:failed]  += results[:counts][:failed]
    aggregate_results[:counts][:ignored] += results[:counts][:ignored]
    aggregate_results[:counts][:stdout]  += results[:stdout].size
    aggregate_results[:time] += results[:time]
  end


  def run_report(stream, template, hash, verbosity)
    output = ERB.new(template, 0, "%<>")
    @streaminator.stream_puts(stream, output.result(binding()), verbosity)
  end
  
end
