
class CacheinatorHelper

  constructor :file_wrapper, :yaml_wrapper
  
  def diff_cached_config?(cached_filepath, hash)
    return true if ( not @file_wrapper.exist?(cached_filepath) )
    return true if ( (@file_wrapper.exist?(cached_filepath)) and (!(@yaml_wrapper.load(cached_filepath) == hash)) )
    return false
  end
  
end
