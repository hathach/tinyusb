require 'ceedling/plugin'
require 'fff_mock_generator'

class FakeFunctionFramework < Plugin

  # Set up Ceedling to use this plugin.
  def setup
    # Get the location of this plugin.
    @plugin_root = File.expand_path(File.join(File.dirname(__FILE__), '..'))
    puts "Using fake function framework (fff)..."

    # Switch out the cmock_builder with our own.
    @ceedling[:cmock_builder].cmock = FffMockGeneratorForCMock.new(@ceedling[:setupinator].config_hash[:cmock])

    # Add the path to fff.h to the include paths.
    COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE_VENDOR << "#{@plugin_root}/vendor/fff"
    COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE_VENDOR << "#{@plugin_root}/src"
  end

  def post_runner_generate(arg_hash)
    # After the test runner file has been created, append the FFF globals
    # definition to the end of the test runner. These globals will be shared by
    # all mocks linked into the test.
    File.open(arg_hash[:runner_file], 'a') do |f|
      f.puts
      f.puts "//=======Defintions of FFF variables====="
      f.puts %{#include "fff.h"}
      f.puts "DEFINE_FFF_GLOBALS;"
    end
  end

end # class FakeFunctionFramework

class FffMockGeneratorForCMock

    def initialize(options=nil)
    @cm_config      = CMockConfig.new(options)
    @cm_parser      = CMockHeaderParser.new(@cm_config)
    @silent        = (@cm_config.verbosity < 2)

    # These are the additional files to include in the mock files.
    @includes_h_pre_orig_header  = (@cm_config.includes || @cm_config.includes_h_pre_orig_header || []).map{|h| h =~ /</ ? h : "\"#{h}\""}
    @includes_h_post_orig_header = (@cm_config.includes_h_post_orig_header || []).map{|h| h =~ /</ ? h : "\"#{h}\""}
    @includes_c_pre_header       = (@cm_config.includes_c_pre_header || []).map{|h| h =~ /</ ? h : "\"#{h}\""}
    @includes_c_post_header      = (@cm_config.includes_c_post_header || []).map{|h| h =~ /</ ? h : "\"#{h}\""}
  end

  def setup_mocks(files)
    [files].flatten.each do |src|
      generate_mock (src)
    end
  end

  def generate_mock (header_file_to_mock)
      module_name = File.basename(header_file_to_mock, '.h')
      puts "Creating mock for #{module_name}..." unless @silent
      mock_name = @cm_config.mock_prefix + module_name + @cm_config.mock_suffix
      mock_path = @cm_config.mock_path
      if @cm_config.subdir
          # If a subdirectory has been configured, append it to the mock path.
          mock_path = "#{mock_path}/#{@cm_config.subdir}"
      end
      full_path_for_mock = "#{mock_path}/#{mock_name}"

      # Parse the header file so we know what to mock.
      parsed_header = @cm_parser.parse(module_name, File.read(header_file_to_mock))

      # Create the directory if it doesn't exist.
      mkdir_p full_path_for_mock.pathmap("%d")

      # Generate the mock header file.
      puts "Creating mock: #{full_path_for_mock}.h"

      # Create the mock header.
      File.open("#{full_path_for_mock}.h", 'w') do |f|
        f.write(FffMockGenerator.create_mock_header(module_name, mock_name, parsed_header,
          @includes_h_pre_orig_header, @includes_h_post_orig_header))
      end

      # Create the mock source file.
      File.open("#{full_path_for_mock}.c", 'w') do |f|
        f.write(FffMockGenerator.create_mock_source(mock_name, parsed_header,
          @includes_c_pre_orig_header, @includes_c_post_orig_header))
      end
  end

end
