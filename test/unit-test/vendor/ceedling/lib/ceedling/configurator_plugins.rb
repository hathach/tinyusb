require 'ceedling/constants'

class ConfiguratorPlugins

  constructor :stream_wrapper, :file_wrapper, :system_wrapper
  attr_reader :rake_plugins, :script_plugins

  def setup
    @rake_plugins   = []
    @script_plugins = []
  end


  def add_load_paths(config)
    plugin_paths = {}

    config[:plugins][:enabled].each do |plugin|
      config[:plugins][:load_paths].each do |root|
        path = File.join(root, plugin)

        is_script_plugin = ( not @file_wrapper.directory_listing( File.join( path, 'lib', '*.rb' ) ).empty? )
        is_rake_plugin = ( not @file_wrapper.directory_listing( File.join( path, '*.rake' ) ).empty? )

        if is_script_plugin or is_rake_plugin
          plugin_paths[(plugin + '_path').to_sym] = path

          if is_script_plugin
            @system_wrapper.add_load_path( File.join( path, 'lib') )
            @system_wrapper.add_load_path( File.join( path, 'config') )
          end
          break
        end
      end
    end

    return plugin_paths
  end


  # gather up and return .rake filepaths that exist on-disk
  def find_rake_plugins(config, plugin_paths)
    @rake_plugins = []
    plugins_with_path = []

    config[:plugins][:enabled].each do |plugin|
      if path = plugin_paths[(plugin + '_path').to_sym]
        rake_plugin_path = File.join(path, "#{plugin}.rake")
        if (@file_wrapper.exist?(rake_plugin_path))
          plugins_with_path << rake_plugin_path
          @rake_plugins << plugin
        end
      end
    end

    return plugins_with_path
  end


  # gather up and return just names of .rb classes that exist on-disk
  def find_script_plugins(config, plugin_paths)
    @script_plugins = []

    config[:plugins][:enabled].each do |plugin|
      if path = plugin_paths[(plugin + '_path').to_sym]
        script_plugin_path = File.join(path, "lib", "#{plugin}.rb")

        if @file_wrapper.exist?(script_plugin_path)
          @script_plugins << plugin
        end
      end
    end

    return @script_plugins
  end


  # gather up and return configuration .yml filepaths that exist on-disk
  def find_config_plugins(config, plugin_paths)
    plugins_with_path = []

    config[:plugins][:enabled].each do |plugin|
      if path = plugin_paths[(plugin + '_path').to_sym]
        config_plugin_path = File.join(path, "config", "#{plugin}.yml")

        if @file_wrapper.exist?(config_plugin_path)
          plugins_with_path << config_plugin_path
        end
      end
    end

    return plugins_with_path
  end


  # gather up and return default .yml filepaths that exist on-disk
  def find_plugin_yml_defaults(config, plugin_paths)
    defaults_with_path = []

    config[:plugins][:enabled].each do |plugin|
      if path = plugin_paths[(plugin + '_path').to_sym]
        default_path = File.join(path, 'config', 'defaults.yml')

        if @file_wrapper.exist?(default_path)
          defaults_with_path << default_path
        end
      end
    end

    return defaults_with_path
  end

  # gather up and return 
  def find_plugin_hash_defaults(config, plugin_paths)
    defaults_hash= []

    config[:plugins][:enabled].each do |plugin|
      if path = plugin_paths[(plugin + '_path').to_sym]
        default_path = File.join(path, "config", "defaults_#{plugin}.rb")
        if @file_wrapper.exist?(default_path)
          @system_wrapper.require_file( "defaults_#{plugin}.rb")

          object = eval("get_default_config()")
          defaults_hash << object
        end
      end
    end

    return defaults_hash
  end

end
