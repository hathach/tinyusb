
class Cacheinator

  constructor :cacheinator_helper, :file_path_utils, :file_wrapper, :yaml_wrapper

  def cache_test_config(hash)
    @yaml_wrapper.dump( @file_path_utils.form_test_build_cache_path( INPUT_CONFIGURATION_CACHE_FILE), hash )
  end

  def cache_release_config(hash)
    @yaml_wrapper.dump( @file_path_utils.form_release_build_cache_path( INPUT_CONFIGURATION_CACHE_FILE ), hash )
  end


  def diff_cached_test_file( filepath )
    cached_filepath = @file_path_utils.form_test_build_cache_path( filepath )

    if (@file_wrapper.exist?( cached_filepath ) and (!@file_wrapper.compare( filepath, cached_filepath )))
      @file_wrapper.cp(filepath, cached_filepath, {:preserve => false})
      return filepath
    elsif (!@file_wrapper.exist?( cached_filepath ))
      @file_wrapper.cp(filepath, cached_filepath, {:preserve => false})
      return filepath
    end

    return cached_filepath
  end

  def diff_cached_test_config?(hash)
    cached_filepath = @file_path_utils.form_test_build_cache_path(INPUT_CONFIGURATION_CACHE_FILE)

    return @cacheinator_helper.diff_cached_config?( cached_filepath, hash )
  end

  def diff_cached_test_defines?(files)
    cached_filepath = @file_path_utils.form_test_build_cache_path(DEFINES_DEPENDENCY_CACHE_FILE)

    return @cacheinator_helper.diff_cached_defines?( cached_filepath, files )
  end

  def diff_cached_release_config?(hash)
    cached_filepath = @file_path_utils.form_release_build_cache_path(INPUT_CONFIGURATION_CACHE_FILE)

    return @cacheinator_helper.diff_cached_config?( cached_filepath, hash )
  end

end
