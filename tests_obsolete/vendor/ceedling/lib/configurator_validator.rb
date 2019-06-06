require 'rubygems'
require 'rake' # for ext()
require 'constants'
require 'tool_executor'    # for argument replacement pattern
require 'file_path_utils'  # for glob handling class methods


class ConfiguratorValidator
  
  constructor :file_wrapper, :stream_wrapper, :system_wrapper

  # walk into config hash verify existence of data at key depth
  def exists?(config, *keys)
    hash  = retrieve_value(config, keys)
    exist = !hash[:value].nil?

    if (not exist)
      # no verbosity checking since this is lowest level anyhow & verbosity checking depends on configurator
      @stream_wrapper.stderr_puts("ERROR: Required config file entry #{format_key_sequence(keys, hash[:depth])} does not exist.")    
    end
    
    return exist
  end


  # walk into config hash. verify directory path(s) at given key depth
  def validate_path_list(config, *keys)
    hash = retrieve_value(config, keys)
    list = hash[:value]

    # return early if we couldn't walk into hash and find a value
    return false if (list.nil?)

    path_list = []
    exist = true
    
    case list
      when String then path_list << list
      when Array  then path_list =  list
    end
    
    path_list.each do |path|
      base_path = FilePathUtils::extract_path(path) # lop off add/subtract notation & glob specifiers
      
      if (not @file_wrapper.exist?(base_path))
        # no verbosity checking since this is lowest level anyhow & verbosity checking depends on configurator
        @stream_wrapper.stderr_puts("ERROR: Config path #{format_key_sequence(keys, hash[:depth])}['#{base_path}'] does not exist on disk.") 
        exist = false
      end 
    end
    
    return exist
  end

  
  # simple path verification
  def validate_filepath_simple(path, *keys)
    validate_path = path
    
    if (not @file_wrapper.exist?(validate_path))
      # no verbosity checking since this is lowest level anyhow & verbosity checking depends on configurator
      @stream_wrapper.stderr_puts("ERROR: Config path '#{validate_path}' associated with #{format_key_sequence(keys, keys.size)} does not exist on disk.") 
      return false
    end 
    
    return true
  end
 
  # walk into config hash. verify specified file exists.
  def validate_filepath(config, *keys)
    hash          = retrieve_value(config, keys)
    filepath      = hash[:value]

    # return early if we couldn't walk into hash and find a value
    return false if (filepath.nil?)

    # skip everything if we've got an argument replacement pattern
    return true if (filepath =~ TOOL_EXECUTOR_ARGUMENT_REPLACEMENT_PATTERN)
    
    if (not @file_wrapper.exist?(filepath))
      # no verbosity checking since this is lowest level anyhow & verbosity checking depends on configurator
      @stream_wrapper.stderr_puts("ERROR: Config filepath #{format_key_sequence(keys, hash[:depth])}['#{filepath}'] does not exist on disk.") 
      return false
    end      

    return true
  end

  # walk into config hash. verify specified file exists.
  def validate_executable_filepath(config, *keys)
    exe_extension = config[:extension][:executable]
    hash          = retrieve_value(config, keys)
    filepath      = hash[:value]

    # return early if we couldn't walk into hash and find a value
    return false if (filepath.nil?)

    # skip everything if we've got an argument replacement pattern
    return true if (filepath =~ TOOL_EXECUTOR_ARGUMENT_REPLACEMENT_PATTERN)
    
    # if there's no path included, verify file exists somewhere in system search paths
    if (not filepath.include?('/'))
      exists = false
      
      @system_wrapper.search_paths.each do |path|
        if (@file_wrapper.exist?( File.join(path, filepath)) )
          exists = true
          break
        end
        
        if (@file_wrapper.exist?( (File.join(path, filepath)).ext( exe_extension ) ))
          exists = true
          break
        elsif (@system_wrapper.windows? and @file_wrapper.exist?( (File.join(path, filepath)).ext( EXTENSION_WIN_EXE ) ))
          exists = true
          break
        end
      end
      
      if (not exists)
        # no verbosity checking since this is lowest level anyhow & verbosity checking depends on configurator
        @stream_wrapper.stderr_puts("ERROR: Config filepath #{format_key_sequence(keys, hash[:depth])}['#{filepath}'] does not exist in system search paths.") 
        return false        
      end
      
    # if there is a path included, check that explicit filepath exists
    else
      if (not @file_wrapper.exist?(filepath))
        # no verbosity checking since this is lowest level anyhow & verbosity checking depends on configurator
        @stream_wrapper.stderr_puts("ERROR: Config filepath #{format_key_sequence(keys, hash[:depth])}['#{filepath}'] does not exist on disk.") 
        return false
      end      
    end

    return true
  end
  
  def validate_tool_stderr_redirect(config, tools, tool)
    redirect = config[tools][tool][:stderr_redirect]
    if (redirect.class == Symbol)
      # map constants and force to array of strings for runtime universality across ruby versions
      if (not StdErrRedirect.constants.map{|constant| constant.to_s}.include?(redirect.to_s.upcase))
        error = "ERROR: [:#{tools}][:#{tool}][:stderr_redirect][:#{redirect}] is not a recognized option " +
                "{#{StdErrRedirect.constants.map{|constant| ':' + constant.to_s.downcase}.join(', ')}}."
        @stream_wrapper.stderr_puts(error) 
        return false        
      end
    end
    
    return true
  end
  
  private #########################################
  
  
  def retrieve_value(config, keys)
    value = nil
    hash  = config
    depth = 0

    # walk into hash & extract value at requested key sequence
    keys.each do |symbol|
      depth += 1
      if (not hash[symbol].nil?)
        hash  = hash[symbol]
        value = hash
      else
        value = nil
        break
      end
    end
    
    return {:value => value, :depth => depth}
  end


  def format_key_sequence(keys, depth)
    walked_keys    = keys.slice(0, depth)
    formatted_keys = walked_keys.map{|key| "[:#{key.to_s}]"}
    
    return formatted_keys.join
  end
  
end
