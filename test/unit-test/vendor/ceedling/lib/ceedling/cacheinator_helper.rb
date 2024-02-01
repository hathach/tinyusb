
class CacheinatorHelper

  constructor :file_wrapper, :yaml_wrapper

  def diff_cached_config?(cached_filepath, hash)
    return false if ( not @file_wrapper.exist?(cached_filepath) )
    return true if (@yaml_wrapper.load(cached_filepath) != hash)
    return false
  end

  def diff_cached_defines?(cached_filepath, files)
    changed_defines = false
    current_defines = COLLECTION_DEFINES_TEST_AND_VENDOR.reject(&:empty?)

    current_dependencies = Hash[files.collect { |source| [source, current_defines.dup] }]
    if not @file_wrapper.exist?(cached_filepath)
      @yaml_wrapper.dump(cached_filepath, current_dependencies)
      return changed_defines
    end

    dependencies = @yaml_wrapper.load(cached_filepath)
    common_dependencies = current_dependencies.select { |file, defines| dependencies.has_key?(file) }

    if dependencies.values_at(*common_dependencies.keys) != common_dependencies.values
      changed_defines = true
    end

    dependencies.merge!(current_dependencies)
    @yaml_wrapper.dump(cached_filepath, dependencies)

    return changed_defines
  end

end
