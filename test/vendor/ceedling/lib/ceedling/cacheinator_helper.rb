
class CacheinatorHelper

  constructor :file_wrapper, :yaml_wrapper
  
  def diff_cached_config?(cached_filepath, hash)
    return true if ( not @file_wrapper.exist?(cached_filepath) )
    return true if ( (@file_wrapper.exist?(cached_filepath)) and (!(@yaml_wrapper.load(cached_filepath) == hash)) )
    return false
  end

  def diff_cached_defines?(cached_filepath, files)
    current_defines = COLLECTION_DEFINES_TEST_AND_VENDOR.reject(&:empty?)

    current_dependency = Hash[files.collect { |source| [source, current_defines.dup] }]
    if not @file_wrapper.exist?(cached_filepath)
      @yaml_wrapper.dump(cached_filepath, current_dependency)
      return false
    end

    dependencies = @yaml_wrapper.load(cached_filepath)
    if dependencies.values_at(*current_dependency.keys) != current_dependency.values
      dependencies.merge!(current_dependency)
      @yaml_wrapper.dump(cached_filepath, dependencies)
      return true
    end

    return false
  end

end
