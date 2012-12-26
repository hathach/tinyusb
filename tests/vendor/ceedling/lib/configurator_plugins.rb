require 'constants'

class ConfiguratorPlugins

  constructor :stream_wrapper, :file_wrapper, :system_wrapper
  attr_reader :rake_plugins, :script_plugins

  def setup
    @rake_plugins   = []
    @script_plugins = []
  end

  
  def add_load_paths(config)
    plugin_paths = {}
    
    config[:plugins][:load_paths].each do |root|
      @system_wrapper.add_load_path( root ) if ( not @file_wrapper.directory_listing( File.join( root, '*.rb' ) ).empty? )
    
      config[:plugins][:enabled].each do |plugin|
        path = File.join(root, plugin, "lib")
        old_path = File.join( root, plugin )

        if ( not @file_wrapper.directory_listing( File.join( path, '*.rb' ) ).empty? )
          plugin_paths[(plugin + '_path').to_sym] = path
          @system_wrapper.add_load_path( path )
        elsif ( not @file_wrapper.directory_listing( File.join( old_path, '*.rb' ) ).empty? )
          plugin_paths[(plugin + '_path').to_sym] = old_path
          @system_wrapper.add_load_path( old_path )
        end
      end
    end
    
    return plugin_paths
  end
  
  
  # gather up and return .rake filepaths that exist on-disk
  def find_rake_plugins(config)
    plugins_with_path = []
    
    config[:plugins][:load_paths].each do |root|
      config[:plugins][:enabled].each do |plugin|
        rake_plugin_path = File.join(root, plugin, "#{plugin}.rake")
        if (@file_wrapper.exist?(rake_plugin_path))
          plugins_with_path << rake_plugin_path
          @rake_plugins << plugin
        end
      end
    end
    
    return plugins_with_path
  end


  # gather up and return just names of .rb classes that exist on-disk
  def find_script_plugins(config)
    config[:plugins][:load_paths].each do |root|
      config[:plugins][:enabled].each do |plugin|
        script_plugin_path = File.join(root, plugin, "lib", "#{plugin}.rb")

        # Add the old path here to support legacy style. Eventaully remove.
        old_script_plugin_path = File.join(root, plugin, "#{plugin}.rb")

        if @file_wrapper.exist?(script_plugin_path) or @file_wrapper.exist?(old_script_plugin_path)
          @script_plugins << plugin 
        end

        # Print depreciation warning.
        if @file_wrapper.exist?(old_script_plugin_path)
          $stderr.puts "WARNING: Depreciated plugin style used in #{plugin}. Use new directory structure!"
        end
      end
    end
    
    return @script_plugins 
  end
  
  
  # gather up and return configuration .yml filepaths that exist on-disk
  def find_config_plugins(config)
    plugins_with_path = []
    
    config[:plugins][:load_paths].each do |root|
      config[:plugins][:enabled].each do |plugin|
        config_plugin_path = File.join(root, plugin, "config", "#{plugin}.yml")

        # Add the old path here to support legacy style. Eventaully remove.
        old_config_plugin_path = File.join(root, plugin, "#{plugin}.yml")

        if @file_wrapper.exist?(config_plugin_path)
          plugins_with_path << config_plugin_path
        elsif @file_wrapper.exist?(old_config_plugin_path)
          # there's a warning printed for this in find_script_plugins
          plugins_with_path << old_config_plugin_path
        end
      end
    end
    
    return plugins_with_path    
  end

  
  # gather up and return default .yml filepaths that exist on-disk
  def find_plugin_defaults(config)
    defaults_with_path = []
    
    config[:plugins][:load_paths].each do |root|
      config[:plugins][:enabled].each do |plugin|
        default_path = File.join(root, plugin, 'config', 'defaults.yml')
        old_default_path = File.join(root, plugin, 'defaults.yml')

        if @file_wrapper.exist?(default_path)
          defaults_with_path << default_path
        elsif @file_wrapper.exist?(old_default_path)
          defaults_with_path << old_default_path
        end
      end
    end

    return defaults_with_path    
  end
  
end
