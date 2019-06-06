
class TestIncludesExtractor

  constructor :configurator, :yaml_wrapper, :file_wrapper


  def setup
    @includes  = {}
    @mocks     = {}
  end


  # for includes_list file, slurp up array from yaml file and sort & store includes
  def parse_includes_list(includes_list)
    gather_and_store_includes( includes_list, @yaml_wrapper.load(includes_list) )
  end

  # open, scan for, and sort & store includes of test file
  def parse_test_file(test)
    gather_and_store_includes( test, extract_from_file(test) )
  end

  # mocks with no file extension
  def lookup_raw_mock_list(test)
    file_key = form_file_key(test)
    return [] if @mocks[file_key].nil?
    return @mocks[file_key]
  end
  
  # includes with file extension
  def lookup_includes_list(file)
    file_key = form_file_key(file)
    return [] if (@includes[file_key]).nil?
    return @includes[file_key]
  end
  
  private #################################
  
  def form_file_key(filepath)
    return File.basename(filepath).to_sym
  end

  def extract_from_file(file)
    includes = []
    header_extension = @configurator.extension_header
    
    contents = @file_wrapper.read(file)

    # remove line comments
    contents = contents.gsub(/\/\/.*$/, '')
    # remove block comments
    contents = contents.gsub(/\/\*.*?\*\//m, '')
    
    contents.split("\n").each do |line|
      # look for include statement
      scan_results = line.scan(/#include\s+\"\s*(.+#{'\\'+header_extension})\s*\"/)
      
      includes << scan_results[0][0] if (scan_results.size > 0)
    end
    
    return includes.uniq
  end

  def gather_and_store_includes(file, includes)
    mock_prefix      = @configurator.cmock_mock_prefix
    header_extension = @configurator.extension_header
    file_key         = form_file_key(file)
    @mocks[file_key] = []
      
    # add includes to lookup hash
    @includes[file_key] = includes
      
    includes.each do |include_file|          
      # check if include is a mock
      scan_results = include_file.scan(/(#{mock_prefix}.+)#{'\\'+header_extension}/)
      # add mock to lookup hash
      @mocks[file_key] << scan_results[0][0] if (scan_results.size > 0)
    end
  end
  
end
