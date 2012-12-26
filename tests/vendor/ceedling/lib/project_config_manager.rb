require 'constants'


class ProjectConfigManager

  attr_reader   :options_files, :release_config_changed, :test_config_changed
  attr_accessor :config_hash

  constructor :cacheinator, :yaml_wrapper


  def setup
    @options_files = []
    @release_config_changed = false
    @test_config_changed    = false
  end


  def merge_options(config_hash, option_filepath)
    @options_files << File.basename( option_filepath )
    config_hash.deep_merge( @yaml_wrapper.load( option_filepath ) )
    return config_hash
  end 
  

  
  def process_release_config_change
    # has project configuration changed since last release build
    @release_config_changed = @cacheinator.diff_cached_release_config?( @config_hash )
  end


  def process_test_config_change
    # has project configuration changed since last test build
    @test_config_changed = @cacheinator.diff_cached_test_config?( @config_hash )
  end

end
