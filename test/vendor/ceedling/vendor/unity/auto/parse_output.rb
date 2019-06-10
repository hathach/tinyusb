#============================================================
#  Author: John Theofanopoulos
#  A simple parser. Takes the output files generated during the
#  build process and extracts information relating to the tests.
#
#  Notes:
#    To capture an output file under VS builds use the following:
#      devenv [build instructions] > Output.txt & type Output.txt
#
#    To capture an output file under Linux builds use the following:
#      make | tee Output.txt
#
#    This script can handle the following output formats:
#    - normal output (raw unity)
#    - fixture output (unity_fixture.h/.c)
#    - fixture output with verbose flag set ("-v")
#
#    To use this parser use the following command
#    ruby parseOutput.rb [options] [file]
#        options: -xml  : produce a JUnit compatible XML file
#           file: file to scan for results
#============================================================

# Parser class for handling the input file
class ParseOutput
  def initialize
    # internal data
    @class_name_idx = 0
    @path_delim = nil

    # xml output related
    @xml_out = false
    @array_list = false

    # current suite name and statistics
    @test_suite = nil
    @total_tests  = 0
    @test_passed  = 0
    @test_failed  = 0
    @test_ignored = 0
  end

  # Set the flag to indicate if there will be an XML output file or not
  def set_xml_output
    @xml_out = true
  end

  # If write our output to XML
  def write_xml_output
    output = File.open('report.xml', 'w')
    output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    @array_list.each do |item|
      output << item << "\n"
    end
  end

  # Pushes the suite info as xml to the array list, which will be written later
  def push_xml_output_suite_info
    # Insert opening tag at front
    heading = '<testsuite name="Unity" tests="' + @total_tests.to_s + '" failures="' + @test_failed.to_s + '"' + ' skips="' + @test_ignored.to_s + '">'
    @array_list.insert(0, heading)
    # Push back the closing tag
    @array_list.push '</testsuite>'
  end

  # Pushes xml output data to the array list, which will be written later
  def push_xml_output_passed(test_name)
    @array_list.push '    <testcase classname="' + @test_suite + '" name="' + test_name + '"/>'
  end

  # Pushes xml output data to the array list, which will be written later
  def push_xml_output_failed(test_name, reason)
    @array_list.push '    <testcase classname="' + @test_suite + '" name="' + test_name + '">'
    @array_list.push '        <failure type="ASSERT FAILED">' + reason + '</failure>'
    @array_list.push '    </testcase>'
  end

  # Pushes xml output data to the array list, which will be written later
  def push_xml_output_ignored(test_name, reason)
    @array_list.push '    <testcase classname="' + @test_suite + '" name="' + test_name + '">'
    @array_list.push '        <skipped type="TEST IGNORED">' + reason + '</skipped>'
    @array_list.push '    </testcase>'
  end

  # This function will try and determine when the suite is changed. This is
  # is the name that gets added to the classname parameter.
  def test_suite_verify(test_suite_name)
    # Split the path name
    test_name = test_suite_name.split(@path_delim)

    # Remove the extension and extract the base_name
    base_name = test_name[test_name.size - 1].split('.')[0]

    # Return if the test suite hasn't changed
    return unless base_name.to_s != @test_suite.to_s

    @test_suite = base_name
    printf "New Test: %s\n", @test_suite
  end

  # Prepares the line for verbose fixture output ("-v")
  def prepare_fixture_line(line)
    line = line.sub('IGNORE_TEST(', '')
    line = line.sub('TEST(', '')
    line = line.sub(')', ',')
    line = line.chomp
    array = line.split(',')
    array.map { |x| x.to_s.lstrip.chomp }
  end

  # Test was flagged as having passed so format the output.
  # This is using the Unity fixture output and not the original Unity output.
  def test_passed_unity_fixture(array)
    class_name = array[0]
    test_name  = array[1]
    test_suite_verify(class_name)
    printf "%-40s PASS\n", test_name

    push_xml_output_passed(test_name) if @xml_out
  end

  # Test was flagged as having failed so format the output.
  # This is using the Unity fixture output and not the original Unity output.
  def test_failed_unity_fixture(array)
    class_name = array[0]
    test_name  = array[1]
    test_suite_verify(class_name)
    reason_array = array[2].split(':')
    reason = reason_array[-1].lstrip.chomp + ' at line: ' + reason_array[-4]

    printf "%-40s FAILED\n", test_name

    push_xml_output_failed(test_name, reason) if @xml_out
  end

  # Test was flagged as being ignored so format the output.
  # This is using the Unity fixture output and not the original Unity output.
  def test_ignored_unity_fixture(array)
    class_name = array[0]
    test_name  = array[1]
    reason = 'No reason given'
    if array.size > 2
      reason_array = array[2].split(':')
      tmp_reason = reason_array[-1].lstrip.chomp
      reason = tmp_reason == 'IGNORE' ? 'No reason given' : tmp_reason
    end
    test_suite_verify(class_name)
    printf "%-40s IGNORED\n", test_name

    push_xml_output_ignored(test_name, reason) if @xml_out
  end

  # Test was flagged as having passed so format the output
  def test_passed(array)
    last_item = array.length - 1
    test_name = array[last_item - 1]
    test_suite_verify(array[@class_name_idx])
    printf "%-40s PASS\n", test_name

    return unless @xml_out

    push_xml_output_passed(test_name) if @xml_out
  end

  # Test was flagged as having failed so format the line
  def test_failed(array)
    last_item = array.length - 1
    test_name = array[last_item - 2]
    reason = array[last_item].chomp.lstrip + ' at line: ' + array[last_item - 3]
    class_name = array[@class_name_idx]

    if test_name.start_with? 'TEST('
      array2 = test_name.split(' ')

      test_suite = array2[0].sub('TEST(', '')
      test_suite = test_suite.sub(',', '')
      class_name = test_suite

      test_name = array2[1].sub(')', '')
    end

    test_suite_verify(class_name)
    printf "%-40s FAILED\n", test_name

    push_xml_output_failed(test_name, reason) if @xml_out
  end

  # Test was flagged as being ignored so format the output
  def test_ignored(array)
    last_item = array.length - 1
    test_name = array[last_item - 2]
    reason = array[last_item].chomp.lstrip
    class_name = array[@class_name_idx]

    if test_name.start_with? 'TEST('
      array2 = test_name.split(' ')

      test_suite = array2[0].sub('TEST(', '')
      test_suite = test_suite.sub(',', '')
      class_name = test_suite

      test_name = array2[1].sub(')', '')
    end

    test_suite_verify(class_name)
    printf "%-40s IGNORED\n", test_name

    push_xml_output_ignored(test_name, reason) if @xml_out
  end

  # Adjusts the os specific members according to the current path style
  # (Windows or Unix based)
  def set_os_specifics(line)
    if line.include? '\\'
      # Windows X:\Y\Z
      @class_name_idx = 1
      @path_delim = '\\'
    else
      # Unix Based /X/Y/Z
      @class_name_idx = 0
      @path_delim = '/'
    end
  end

  # Main function used to parse the file that was captured.
  def process(file_name)
    @array_list = []

    puts 'Parsing file: ' + file_name

    @test_passed = 0
    @test_failed = 0
    @test_ignored = 0
    puts ''
    puts '=================== RESULTS ====================='
    puts ''
    File.open(file_name).each do |line|
      # Typical test lines look like these:
      # ----------------------------------------------------
      # 1. normal output:
      # <path>/<test_file>.c:36:test_tc1000_opsys:FAIL: Expected 1 Was 0
      # <path>/<test_file>.c:112:test_tc5004_initCanChannel:IGNORE: Not Yet Implemented
      # <path>/<test_file>.c:115:test_tc5100_initCanVoidPtrs:PASS
      #
      # 2. fixture output
      # <path>/<test_file>.c:63:TEST(<test_group>, <test_function>):FAIL: Expected 0x00001234 Was 0x00005A5A
      # <path>/<test_file>.c:36:TEST(<test_group>, <test_function>):IGNORE
      # Note: "PASS" information won't be generated in this mode
      #
      # 3. fixture output with verbose information ("-v")
      # TEST(<test_group, <test_file>)<path>/<test_file>:168::FAIL: Expected 0x8D Was 0x8C
      # TEST(<test_group>, <test_file>)<path>/<test_file>:22::IGNORE: This Test Was Ignored On Purpose
      # IGNORE_TEST(<test_group, <test_file>)
      # TEST(<test_group, <test_file>) PASS
      #
      # Note: Where path is different on Unix vs Windows devices (Windows leads with a drive letter)!
      set_os_specifics(line)
      line_array = line.split(':')

      # If we were able to split the line then we can look to see if any of our target words
      # were found. Case is important.
      if (line_array.size >= 4) || (line.start_with? 'TEST(') || (line.start_with? 'IGNORE_TEST(')

        # check if the output is fixture output (with verbose flag "-v")
        if (line.start_with? 'TEST(') || (line.start_with? 'IGNORE_TEST(')
          line_array = prepare_fixture_line(line)
          if line.include? ' PASS'
            test_passed_unity_fixture(line_array)
            @test_passed += 1
          elsif line.include? 'FAIL'
            test_failed_unity_fixture(line_array)
            @test_failed += 1
          elsif line.include? 'IGNORE'
            test_ignored_unity_fixture(line_array)
            @test_ignored += 1
          end
        # normal output / fixture output (without verbose "-v")
        elsif line.include? ':PASS'
          test_passed(line_array)
          @test_passed += 1
        elsif line.include? ':FAIL'
          test_failed(line_array)
          @test_failed += 1
        elsif line.include? ':IGNORE:'
          test_ignored(line_array)
          @test_ignored += 1
        elsif line.include? ':IGNORE'
          line_array.push('No reason given')
          test_ignored(line_array)
          @test_ignored += 1
        end
        @total_tests = @test_passed + @test_failed + @test_ignored
      end
    end
    puts ''
    puts '=================== SUMMARY ====================='
    puts ''
    puts 'Tests Passed  : ' + @test_passed.to_s
    puts 'Tests Failed  : ' + @test_failed.to_s
    puts 'Tests Ignored : ' + @test_ignored.to_s

    return unless @xml_out

    # push information about the suite
    push_xml_output_suite_info
    # write xml output file
    write_xml_output
  end
end

# If the command line has no values in, used a default value of Output.txt
parse_my_file = ParseOutput.new

if ARGV.size >= 1
  ARGV.each do |arg|
    if arg == '-xml'
      parse_my_file.set_xml_output
    else
      parse_my_file.process(arg)
      break
    end
  end
end
