require 'ceedling/plugin'
require 'ceedling/constants'

class RawOutputReport < Plugin
  def setup
    @log_paths = {}
  end

  def post_test_fixture_execute(arg_hash)
    output = strip_output(arg_hash[:shell_result][:output])
    write_raw_output_log(arg_hash, output)
  end

  private

  def strip_output(raw_output)
    output = ""
    raw_output.each_line do |line|
      next if line =~ /^\n$/
      next if line =~ /^.*:\d+:.*:(IGNORE|PASS|FAIL)/
      return output if line =~/^-----------------------\n$/
      output << line
    end
  end
  def write_raw_output_log(arg_hash, output)
    logging = generate_log_path(arg_hash)
    @ceedling[:file_wrapper].write(logging[:path], output , logging[:flags]) unless logging.nil?
  end

  def generate_log_path(arg_hash)
    f_name = File.basename(arg_hash[:result_file], '.pass')
    base_path = File.join(PROJECT_BUILD_ARTIFACTS_ROOT, arg_hash[:context].to_s)
    file_path = File.join(base_path, f_name + '.log')

    if @ceedling[:file_wrapper].exist?(base_path)
      return { path: file_path, flags: 'w' }
    end

    nil
  end
end
