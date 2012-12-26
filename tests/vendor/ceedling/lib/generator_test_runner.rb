
class GeneratorTestRunner

  constructor :configurator, :file_path_utils, :file_wrapper

  def find_test_cases(test_file)
    tests = []
    tests_and_line_numbers = []
    lines = []
    
    # if we don't have preprocessor assistance, do some basic preprocessing of our own
    if (not @configurator.project_use_test_preprocessor)
      source = @file_wrapper.read(test_file)
    
      # remove line comments
      source = source.gsub(/\/\/.*$/, '')
      # remove block comments
      source = source.gsub(/\/\*.*?\*\//m, '')
    
      # treat preprocessor directives as a logical line
      lines = source.split(/(^\s*\#.*$) | (;|\{|\}) /x) # match ;, {, and } as end of lines
    # otherwise, read the preprocessed file raw
    else
      lines = @file_wrapper.read( @file_path_utils.form_preprocessed_file_filepath(test_file) ).split(/;|\{|\}/)
    end
    
    # step 1. find test functions in (possibly preprocessed) file
    # (note that lines are not broken up at end of lines)
    lines.each do |line|
      if (line =~ /^\s*void\s+((T|t)est.*)\s*\(\s*(void)?\s*\)/m)
        tests << ($1.strip)
      end
    end
    
    # step 2. associate test functions with line numbers in (non-preprocessed) original file
    # (note that this time we must scan file contents broken up by end of lines)
    raw_lines = @file_wrapper.read(test_file).split("\n")
    raw_index = 0
    
    tests.each do |test|
      raw_lines[raw_index..-1].each_with_index do |line, index|
        # test function might be declared across lines; look for it by its name followed
        #  by a few tell-tale signs
        if (line =~ /#{test}\s*($|\(|\()/)
          raw_index += (index + 1)
          tests_and_line_numbers << {:test => test, :line_number => raw_index}
          break
        end
      end
    end
    
    return tests_and_line_numbers
  end
  
  def generate(module_name, runner_filepath, test_cases, mock_list)
    require 'generate_test_runner.rb'
    @test_runner_generator ||= UnityTestRunnerGenerator.new( @configurator.get_runner_config )
    @test_runner_generator.generate( module_name, 
                                     runner_filepath, 
                                     test_cases, 
                                     mock_list)
  end
end
