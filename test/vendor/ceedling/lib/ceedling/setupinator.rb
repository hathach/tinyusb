
class Setupinator

  attr_reader :config_hash
  attr_writer :ceedling

  def setup
    @ceedling = {}
    @config_hash = {}
  end

  def load_project_files
    @ceedling[:project_file_loader].find_project_files
    return @ceedling[:project_file_loader].load_project_config
  end

  def do_setup(config_hash)
    @config_hash = config_hash

    # load up all the constants and accessors our rake files, objects, & external scripts will need;
    # note: configurator modifies the cmock section of the hash with a couple defaults to tie 
    #       project together - the modified hash is used to build cmock object
    @ceedling[:configurator].populate_defaults( config_hash )
    @ceedling[:configurator].populate_unity_defaults( config_hash )
    @ceedling[:configurator].populate_cmock_defaults( config_hash )
    @ceedling[:configurator].find_and_merge_plugins( config_hash )
    @ceedling[:configurator].merge_imports( config_hash )
    @ceedling[:configurator].tools_setup( config_hash )
    @ceedling[:configurator].eval_environment_variables( config_hash )
    @ceedling[:configurator].eval_paths( config_hash )
    @ceedling[:configurator].standardize_paths( config_hash )
    @ceedling[:configurator].validate( config_hash )
    @ceedling[:configurator].build( config_hash, :environment )
    
    @ceedling[:configurator].insert_rake_plugins( @ceedling[:configurator].rake_plugins )
    @ceedling[:configurator].tools_supplement_arguments( config_hash )
    
    # merge in any environment variables plugins specify, after the main build
    @ceedling[:plugin_manager].load_plugin_scripts( @ceedling[:configurator].script_plugins, @ceedling ) do |env|
      @ceedling[:configurator].eval_environment_variables( env )
      @ceedling[:configurator].build_supplement( config_hash, env )
    end
    
    @ceedling[:plugin_reportinator].set_system_objects( @ceedling )
    @ceedling[:file_finder].prepare_search_sources
    @ceedling[:loginator].setup_log_filepath
    @ceedling[:project_config_manager].config_hash = config_hash
  end

  def reset_defaults(config_hash)
    @ceedling[:configurator].reset_defaults( config_hash )
  end
end
