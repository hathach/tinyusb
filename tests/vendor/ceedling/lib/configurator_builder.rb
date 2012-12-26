require 'rubygems'
require 'rake'            # for ext() method
require 'file_path_utils' # for class methods
require 'defaults'
require 'constants'       # for Verbosity constants class & base file paths



class ConfiguratorBuilder
  
  constructor :file_system_utils, :file_wrapper, :system_wrapper
    
  
  def build_global_constants(config)
    config.each_pair do |key, value|
      formatted_key = key.to_s.upcase
      # undefine global constant if it already exists
      Object.send(:remove_const, formatted_key.to_sym) if @system_wrapper.constants_include?(formatted_key)
      # create global constant
      Object.module_eval("#{formatted_key} = value")
    end
  end

  
  def build_accessor_methods(config, context)
    config.each_pair do |key, value|
      # fill configurator object with accessor methods
      eval("def #{key.to_s.downcase}() return @project_config_hash[:#{key.to_s}] end", context)
    end
  end

  
  # create a flattened hash from the original configuration structure
  def flattenify(config)
    new_hash = {}
    
    config.each_key do | parent |

      # gracefully handle empty top-level entries
      next if (config[parent].nil?)

      case config[parent]
      when Array
        config[parent].each do |hash|
          key = "#{parent.to_s.downcase}_#{hash.keys[0].to_s.downcase}".to_sym
          new_hash[key] = hash[hash.keys[0]]
        end
      when Hash
        config[parent].each_pair do | child, value |
          key = "#{parent.to_s.downcase}_#{child.to_s.downcase}".to_sym
          new_hash[key] = value
        end
      # handle entries with no children, only values
      else
        new_hash["#{parent.to_s.downcase}".to_sym] = config[parent]
      end
      
    end
    
    return new_hash
  end

  
  def populate_defaults(config, defaults)
    defaults.keys.sort.each do |section|
      defaults[section].keys.sort.each do |entry|
        config[section][entry] = defaults[section][entry].deep_clone if (config[section].nil? or config[section][entry].nil?)
      end
    end
  end


  def clean(in_hash)
    # ensure that include files inserted into test runners have file extensions & proper ones at that
    in_hash[:test_runner_includes].map!{|include| include.ext(in_hash[:extension_header])}
  end


  def set_build_paths(in_hash)
    out_hash = {}

    project_build_artifacts_root = File.join(in_hash[:project_build_root], 'artifacts')
    project_build_tests_root     = File.join(in_hash[:project_build_root], TESTS_BASE_PATH)
    project_build_release_root   = File.join(in_hash[:project_build_root], RELEASE_BASE_PATH)

    paths = [
      [:project_build_artifacts_root,  project_build_artifacts_root, true ],
      [:project_build_tests_root,      project_build_tests_root,     true ],
      [:project_build_release_root,    project_build_release_root,   in_hash[:project_release_build] ],

      [:project_test_artifacts_path,     File.join(project_build_artifacts_root, TESTS_BASE_PATH), true ],
      [:project_test_runners_path,       File.join(project_build_tests_root, 'runners'),           true ],
      [:project_test_results_path,       File.join(project_build_tests_root, 'results'),           true ],
      [:project_test_build_output_path,  File.join(project_build_tests_root, 'out'),               true ],
      [:project_test_build_cache_path,   File.join(project_build_tests_root, 'cache'),             true ],
      [:project_test_dependencies_path,  File.join(project_build_tests_root, 'dependencies'),      true ],

      [:project_release_artifacts_path,         File.join(project_build_artifacts_root, RELEASE_BASE_PATH), in_hash[:project_release_build] ],
      [:project_release_build_cache_path,       File.join(project_build_release_root, 'cache'),             in_hash[:project_release_build] ],
      [:project_release_build_output_path,      File.join(project_build_release_root, 'out'),               in_hash[:project_release_build] ],
      [:project_release_build_output_asm_path,  File.join(project_build_release_root, 'out', 'asm'),        in_hash[:project_release_build] ],
      [:project_release_build_output_c_path,    File.join(project_build_release_root, 'out', 'c'),          in_hash[:project_release_build] ],
      [:project_release_dependencies_path,      File.join(project_build_release_root, 'dependencies'),      in_hash[:project_release_build] ],

      [:project_log_path,   File.join(in_hash[:project_build_root], 'logs'), true ],
      [:project_temp_path,  File.join(in_hash[:project_build_root], 'temp'), true ],

      [:project_test_preprocess_includes_path,  File.join(project_build_tests_root, 'preprocess/includes'), in_hash[:project_use_test_preprocessor] ],
      [:project_test_preprocess_files_path,     File.join(project_build_tests_root, 'preprocess/files'),    in_hash[:project_use_test_preprocessor] ],
    ]

    out_hash[:project_build_paths] = []

    # fetch already set mock path
    out_hash[:project_build_paths] << in_hash[:cmock_mock_path] if (in_hash[:project_use_mocks])

    paths.each do |path|
      build_path_name          = path[0]
      build_path               = path[1]
      build_path_add_condition = path[2]
      
      # insert path into build paths if associated with true condition
      out_hash[:project_build_paths] << build_path if build_path_add_condition
      # set path symbol name and path for each entry in paths array
      out_hash[build_path_name] = build_path
    end

    return out_hash
  end


  def set_force_build_filepaths(in_hash)
    out_hash = {}
    
    out_hash[:project_test_force_rebuild_filepath]    = File.join( in_hash[:project_test_dependencies_path], 'force_build' )
    out_hash[:project_release_force_rebuild_filepath] = File.join( in_hash[:project_release_dependencies_path], 'force_build' ) if (in_hash[:project_release_build])

    return out_hash
  end


  def set_rakefile_components(in_hash)
    out_hash = {
      :project_rakefile_component_files => 
        [File.join(CEEDLING_LIB, 'tasks_base.rake'),
         File.join(CEEDLING_LIB, 'tasks_filesystem.rake'),
         File.join(CEEDLING_LIB, 'tasks_tests.rake'),
         File.join(CEEDLING_LIB, 'tasks_vendor.rake'),
         File.join(CEEDLING_LIB, 'rules_tests.rake')]}

    out_hash[:project_rakefile_component_files] << File.join(CEEDLING_LIB, 'rules_cmock.rake') if (in_hash[:project_use_mocks])
    out_hash[:project_rakefile_component_files] << File.join(CEEDLING_LIB, 'rules_preprocess.rake') if (in_hash[:project_use_test_preprocessor])
    out_hash[:project_rakefile_component_files] << File.join(CEEDLING_LIB, 'rules_tests_deep_dependencies.rake') if (in_hash[:project_use_deep_dependencies])
    out_hash[:project_rakefile_component_files] << File.join(CEEDLING_LIB, 'tasks_tests_deep_dependencies.rake') if (in_hash[:project_use_deep_dependencies])

    out_hash[:project_rakefile_component_files] << File.join(CEEDLING_LIB, 'rules_release_deep_dependencies.rake') if (in_hash[:project_release_build] and in_hash[:project_use_deep_dependencies])
    out_hash[:project_rakefile_component_files] << File.join(CEEDLING_LIB, 'rules_release.rake') if (in_hash[:project_release_build])
    out_hash[:project_rakefile_component_files] << File.join(CEEDLING_LIB, 'tasks_release_deep_dependencies.rake') if (in_hash[:project_release_build] and in_hash[:project_use_deep_dependencies])
    out_hash[:project_rakefile_component_files] << File.join(CEEDLING_LIB, 'tasks_release.rake') if (in_hash[:project_release_build])

    return out_hash
  end
  

  def set_library_build_info_filepaths(hash)

    # Notes:
    #  - Dependency on a change to our input configuration hash is handled elsewhere as it is 
    #    dynamically formed during ceedling's execution
    #  - Compiled vendor dependencies like cmock.o, unity.o, cexception.o are handled below;
    #    here we're interested only in ceedling-based code generation dependencies
    
    ceedling_build_info_filepath = File.join(CEEDLING_RELEASE, 'build.info')
    cmock_build_info_filepath    = FilePathUtils::form_ceedling_vendor_path('cmock/release', 'build.info')

    out_hash = {
      :ceedling_build_info_filepath => ceedling_build_info_filepath,
      :cmock_build_info_filepath => cmock_build_info_filepath
    }
    
    return out_hash
  end

  
  def set_release_target(in_hash)
    return {} if (not in_hash[:project_release_build])
    
    release_target_file = ((in_hash[:release_build_output].nil?) ? (DEFAULT_RELEASE_TARGET_NAME.ext(in_hash[:extension_executable])) : in_hash[:release_build_output])
    release_map_file    = ((in_hash[:release_build_output].nil?) ? (DEFAULT_RELEASE_TARGET_NAME.ext(in_hash[:extension_map])) : in_hash[:release_build_output].ext(in_hash[:extension_map]))
    
    return {
      # tempted to make a helper method in file_path_utils? stop right there, pal. you'll introduce a cyclical dependency
      :project_release_build_target => File.join(in_hash[:project_build_release_root], release_target_file),
      :project_release_build_map    => File.join(in_hash[:project_build_release_root], release_map_file)
      }
  end
  

  def collect_project_options(in_hash)
    options = []
    
    in_hash[:project_options_paths].each do |path|
      options << @file_wrapper.directory_listing( File.join(path, '*.yml') )
    end
    
    return {
      :collection_project_options => options.flatten
      }
  end
  

  def expand_all_path_globs(in_hash)
    out_hash = {}
    path_keys = []
    
    in_hash.each_key do |key|
      next if (not key.to_s[0..4] == 'paths')
      path_keys << key
    end
    
    # sorted to provide assured order of traversal in test calls on mocks
    path_keys.sort.each do |key|
      out_hash["collection_#{key.to_s}".to_sym] = @file_system_utils.collect_paths( in_hash[key] )
    end
    
    return out_hash
  end


  def collect_source_and_include_paths(in_hash)
    return {
      :collection_paths_source_and_include => 
        ( in_hash[:collection_paths_source] + 
          in_hash[:collection_paths_include] ).select {|x| File.directory?(x)}
      }    
  end


  def collect_source_include_vendor_paths(in_hash)
    extra_paths = []
    extra_paths << FilePathUtils::form_ceedling_vendor_path(CEXCEPTION_LIB_PATH) if (in_hash[:project_use_exceptions])

    return {
      :collection_paths_source_include_vendor => 
        in_hash[:collection_paths_source_and_include] + 
        extra_paths
      }    
  end


  def collect_test_support_source_include_paths(in_hash)
    return {
      :collection_paths_test_support_source_include => 
        (in_hash[:collection_paths_test] +
        in_hash[:collection_paths_support] +
        in_hash[:collection_paths_source] + 
        in_hash[:collection_paths_include] ).select {|x| File.directory?(x)}
      }    
  end


  def collect_vendor_paths(in_hash)
    return {:collection_paths_vendor => get_vendor_paths(in_hash)}
  end
  

  def collect_test_support_source_include_vendor_paths(in_hash)
    return {
      :collection_paths_test_support_source_include_vendor => 
        in_hash[:collection_paths_test_support_source_include] +
        get_vendor_paths(in_hash)
      }    
  end
  
  
  def collect_tests(in_hash)
    all_tests = @file_wrapper.instantiate_file_list

    in_hash[:collection_paths_test].each do |path|
      all_tests.include( File.join(path, "#{in_hash[:project_test_file_prefix]}*#{in_hash[:extension_source]}") )
    end

    @file_system_utils.revise_file_list( all_tests, in_hash[:files_test] )

    return {:collection_all_tests => all_tests}
  end


  def collect_assembly(in_hash)
    all_assembly = @file_wrapper.instantiate_file_list

    return {:collection_all_assembly => all_assembly} if (not in_hash[:release_build_use_assembly])
    
    in_hash[:collection_paths_source].each do |path|
      all_assembly.include( File.join(path, "*#{in_hash[:extension_assembly]}") )
    end
    
    @file_system_utils.revise_file_list( all_assembly, in_hash[:files_assembly] )

    return {:collection_all_assembly => all_assembly}
  end


  def collect_source(in_hash)
    all_source = @file_wrapper.instantiate_file_list
    in_hash[:collection_paths_source].each do |path|
      if File.exists?(path) and not File.directory?(path)
        all_source.include( path )
      else
        all_source.include( File.join(path, "*#{in_hash[:extension_source]}") )
      end
    end
    @file_system_utils.revise_file_list( all_source, in_hash[:files_source] )
    
    return {:collection_all_source => all_source}
  end


  def collect_headers(in_hash)
    all_headers = @file_wrapper.instantiate_file_list

    paths = 
      in_hash[:collection_paths_test] +
      in_hash[:collection_paths_support] +
      in_hash[:collection_paths_source] + 
      in_hash[:collection_paths_include]
    
    paths.each do |path|
      all_headers.include( File.join(path, "*#{in_hash[:extension_header]}") )
    end

    @file_system_utils.revise_file_list( all_headers, in_hash[:files_include] )
    
    return {:collection_all_headers => all_headers}
  end


  def collect_all_existing_compilation_input(in_hash)
    all_input = @file_wrapper.instantiate_file_list

    paths = 
      in_hash[:collection_paths_test] + 
      in_hash[:collection_paths_support] + 
      in_hash[:collection_paths_source] + 
      in_hash[:collection_paths_include] +
      [FilePathUtils::form_ceedling_vendor_path(UNITY_LIB_PATH)]
    
    paths << FilePathUtils::form_ceedling_vendor_path(CEXCEPTION_LIB_PATH) if (in_hash[:project_use_exceptions])
    paths << FilePathUtils::form_ceedling_vendor_path(CMOCK_LIB_PATH) if (in_hash[:project_use_mocks])

    paths.each do |path|
      all_input.include( File.join(path, "*#{in_hash[:extension_header]}") )
      if File.exists?(path) and not File.directory?(path)
        all_input.include( path )
      else
        all_input.include( File.join(path, "*#{in_hash[:extension_source]}") )
      end
    end
    
    @file_system_utils.revise_file_list( all_input, in_hash[:files_test] )
    @file_system_utils.revise_file_list( all_input, in_hash[:files_support] )
    @file_system_utils.revise_file_list( all_input, in_hash[:files_source] )
    @file_system_utils.revise_file_list( all_input, in_hash[:files_include] )
    # finding assembly files handled explicitly through other means

    return {:collection_all_existing_compilation_input => all_input}    
  end


  def collect_test_and_vendor_defines(in_hash)
    test_defines = in_hash[:defines_test].clone

    test_defines.concat(in_hash[:unity_defines])
    test_defines.concat(in_hash[:cmock_defines])      if (in_hash[:project_use_mocks])
    test_defines.concat(in_hash[:cexception_defines]) if (in_hash[:project_use_exceptions])
    
    return {:collection_defines_test_and_vendor => test_defines}
  end


  def collect_release_and_vendor_defines(in_hash)
    release_defines = in_hash[:defines_release].clone
    
    release_defines.concat(in_hash[:cexception_defines]) if (in_hash[:project_use_exceptions])
    
    return {:collection_defines_release_and_vendor => release_defines}
  end


  def collect_release_artifact_extra_link_objects(in_hash)
    objects = []

    # no build paths here so plugins can remap if necessary (i.e. path mapping happens at runtime)
    objects << CEXCEPTION_C_FILE.ext( in_hash[:extension_object] ) if (in_hash[:project_use_exceptions])

    return {:collection_release_artifact_extra_link_objects => objects}
  end
  

  def collect_test_fixture_extra_link_objects(in_hash)
    #  Note: Symbols passed to compiler at command line can change Unity and CException behavior / configuration;
    #    we also handle those dependencies elsewhere in compilation dependencies
    
    objects = [UNITY_C_FILE]
    
    # we don't include paths here because use of plugins or mixing different compilers may require different build paths
    objects << CEXCEPTION_C_FILE if (in_hash[:project_use_exceptions])
    objects << CMOCK_C_FILE      if (in_hash[:project_use_mocks])
    
    # if we're using mocks & a unity helper is defined & that unity helper includes a source file component (not only a header of macros),
    # then link in the unity_helper object file too
    if ( in_hash[:project_use_mocks] and
         in_hash[:cmock_unity_helper] and 
         @file_wrapper.exist?(in_hash[:cmock_unity_helper].ext(in_hash[:extension_source])) )
      objects << File.basename(in_hash[:cmock_unity_helper])
    end

    # no build paths here so plugins can remap if necessary (i.e. path mapping happens at runtime)
    objects.map! { |object| object.ext(in_hash[:extension_object]) }
    
    return { :collection_test_fixture_extra_link_objects => objects }
  end


  private

  def get_vendor_paths(in_hash)
    vendor_paths = []
    vendor_paths << FilePathUtils::form_ceedling_vendor_path(UNITY_LIB_PATH)
    vendor_paths << FilePathUtils::form_ceedling_vendor_path(CEXCEPTION_LIB_PATH) if (in_hash[:project_use_exceptions])
    vendor_paths << FilePathUtils::form_ceedling_vendor_path(CMOCK_LIB_PATH)      if (in_hash[:project_use_mocks])
    vendor_paths << in_hash[:cmock_mock_path]                                     if (in_hash[:project_use_mocks])

    return vendor_paths
  end
  
end
