require 'ceedling/constants'


class ProjectConfigManager

  attr_reader   :options_files, :release_config_changed, :test_config_changed, :test_defines_changed
  attr_accessor :config_hash

  constructor :cacheinator, :configurator, :yaml_wrapper, :file_wrapper


  def setup
    @options_files = []
    @release_config_changed = false
    @test_config_changed    = false
    @test_defines_changed   = false
  end


  def merge_options(config_hash, option_filepath)
    @options_files << File.basename( option_filepath )
    config_hash.deep_merge!( @yaml_wrapper.load( option_filepath ) )
  end 


  def filter_internal_sources(sources)
    filtered_sources = sources.clone
    filtered_sources.delete_if { |item| item =~ /#{CMOCK_MOCK_PREFIX}.+#{Regexp.escape(EXTENSION_SOURCE)}$/ }
    filtered_sources.delete_if { |item| item =~ /#{VENDORS_FILES.map{|source| '\b' + Regexp.escape(source.ext(EXTENSION_SOURCE)) + '\b'}.join('|')}$/ }
    return filtered_sources
  end

  def process_release_config_change
    # has project configuration changed since last release build
    @release_config_changed = @cacheinator.diff_cached_release_config?( @config_hash )
  end


  def process_test_config_change
    # has project configuration changed since last test build
    @test_config_changed = @cacheinator.diff_cached_test_config?( @config_hash )
  end

  def process_test_defines_change(files)
    # has definitions changed since last test build
    @test_defines_changed = @cacheinator.diff_cached_test_defines?( files )
    if @test_defines_changed
      # update timestamp for rake task prerequisites
      @file_wrapper.touch( @configurator.project_test_force_rebuild_filepath, :mtime => Time.now + 10 )
    end
  end
end
