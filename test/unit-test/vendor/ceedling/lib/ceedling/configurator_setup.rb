
# add sort-ability to symbol so we can order keys array in hash for test-ability
class Symbol
  include Comparable

  def <=>(other)
    self.to_s <=> other.to_s
  end
end


class ConfiguratorSetup

  constructor :configurator_builder, :configurator_validator, :configurator_plugins, :stream_wrapper


  def build_project_config(config, flattened_config)
    ### flesh out config
    @configurator_builder.clean(flattened_config)

    ### add to hash values we build up from configuration & file system contents
    flattened_config.merge!(@configurator_builder.set_build_paths(flattened_config))
    flattened_config.merge!(@configurator_builder.set_force_build_filepaths(flattened_config))
    flattened_config.merge!(@configurator_builder.set_rakefile_components(flattened_config))
    flattened_config.merge!(@configurator_builder.set_release_target(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_project_options(flattened_config))

    ### iterate through all entries in paths section and expand any & all globs to actual paths
    flattened_config.merge!(@configurator_builder.expand_all_path_globs(flattened_config))

    flattened_config.merge!(@configurator_builder.collect_vendor_paths(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_source_and_include_paths(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_source_include_vendor_paths(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_test_support_source_include_paths(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_test_support_source_include_vendor_paths(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_tests(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_assembly(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_source(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_headers(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_release_existing_compilation_input(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_all_existing_compilation_input(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_vendor_defines(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_test_and_vendor_defines(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_release_and_vendor_defines(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_release_artifact_extra_link_objects(flattened_config))
    flattened_config.merge!(@configurator_builder.collect_test_fixture_extra_link_objects(flattened_config))

    return flattened_config
  end


  def build_constants_and_accessors(config, context)
    @configurator_builder.build_global_constants(config)
    @configurator_builder.build_accessor_methods(config, context)
  end


  def validate_required_sections(config)
    validation = []
    validation << @configurator_validator.exists?(config, :project)
    validation << @configurator_validator.exists?(config, :paths)

    return false if (validation.include?(false))
    return true
  end

  def validate_required_section_values(config)
    validation = []
    validation << @configurator_validator.exists?(config, :project, :build_root)
    validation << @configurator_validator.exists?(config, :paths, :test)
    validation << @configurator_validator.exists?(config, :paths, :source)

    return false if (validation.include?(false))
    return true
  end

  def validate_paths(config)
    validation = []

    if config[:cmock][:unity_helper]
      config[:cmock][:unity_helper].each do |path|
        validation << @configurator_validator.validate_filepath_simple( path, :cmock, :unity_helper ) 
      end
    end

    config[:project][:options_paths].each do |path|
      validation << @configurator_validator.validate_filepath_simple( path, :project, :options_paths )
    end

    config[:plugins][:load_paths].each do |path|
      validation << @configurator_validator.validate_filepath_simple( path, :plugins, :load_paths )
    end

    config[:paths].keys.sort.each do |key|
      validation << @configurator_validator.validate_path_list(config, :paths, key)
    end

    return false if (validation.include?(false))
    return true
  end

  def validate_tools(config)
    validation = []

    config[:tools].keys.sort.each do |key|
      validation << @configurator_validator.exists?(config, :tools, key, :executable)
      validation << @configurator_validator.validate_executable_filepath(config, :tools, key, :executable) if (not config[:tools][key][:optional])
      validation << @configurator_validator.validate_tool_stderr_redirect(config, :tools, key)
    end

    return false if (validation.include?(false))
    return true
  end

  def validate_plugins(config)
    missing_plugins =
      Set.new( config[:plugins][:enabled] ) -
      Set.new( @configurator_plugins.rake_plugins ) -
      Set.new( @configurator_plugins.script_plugins )

    missing_plugins.each do |plugin|
      @stream_wrapper.stderr_puts("ERROR: Ceedling plugin '#{plugin}' contains no rake or ruby class entry point. (Misspelled or missing files?)")
    end

    return ( (missing_plugins.size > 0) ? false : true )
  end

end
