require 'ceedling/plugin'
require 'ceedling/constants'

DEPENDENCIES_ROOT_NAME         = 'dependencies'
DEPENDENCIES_TASK_ROOT         = DEPENDENCIES_ROOT_NAME + ':'
DEPENDENCIES_SYM               = DEPENDENCIES_ROOT_NAME.to_sym

class Dependencies < Plugin

  def setup
    @plugin_root = File.expand_path(File.join(File.dirname(__FILE__), '..'))

    # Set up a fast way to look up dependencies by name or static lib path
    @dependencies = {}
    @dynamic_libraries = []
    DEPENDENCIES_LIBRARIES.each do |deplib|

      @dependencies[ deplib[:name] ] = deplib.clone
      all_deps = get_static_libraries_for_dependency(deplib) +
                 get_dynamic_libraries_for_dependency(deplib) +
                 get_include_directories_for_dependency(deplib) +
                 get_source_files_for_dependency(deplib)
      all_deps.each do |key|
        @dependencies[key] = @dependencies[ deplib[:name] ]
      end

      @dynamic_libraries += get_dynamic_libraries_for_dependency(deplib)
    end
  end

  def config
    updates = {
      :collection_paths_include => COLLECTION_PATHS_INCLUDE,
      :collection_all_headers => COLLECTION_ALL_HEADERS,
    }

    @ceedling[DEPENDENCIES_SYM].get_include_directories_for_dependency(deplib).each do |incpath|
      updates[:collection_paths_include] << incpath
      Dir[ File.join(incpath, "*#{EXTENSION_HEADER}") ].each do |f|
        updates[:collection_all_headers] << f
      end
    end

    return updates
  end

  def get_name(deplib)
    raise "Each dependency must have a name!" if deplib[:name].nil?
    return deplib[:name].gsub(/\W*/,'')
  end

  def get_source_path(deplib)
    return deplib[:source_path] || File.join('dependencies', get_name(deplib))
  end

  def get_build_path(deplib)
    return deplib[:build_path] || deplib[:source_path] || File.join('dependencies', get_name(deplib))
  end

  def get_artifact_path(deplib)
    return deplib[:artifact_path] || deplib[:source_path] || File.join('dependencies', get_name(deplib))
  end

  def get_working_paths(deplib)
    paths = [deplib[:source_path], deplib[:build_path], deplib[:artifact_paths]].compact.uniq
    paths = [ File.join('dependencies', get_name(deplib)) ] if (paths.empty?)
    return paths
  end

  def get_static_libraries_for_dependency(deplib)
    (deplib[:artifacts][:static_libraries] || []).map {|path| File.join(get_artifact_path(deplib), path)}
  end

  def get_dynamic_libraries_for_dependency(deplib)
    (deplib[:artifacts][:dynamic_libraries] || []).map {|path| File.join(get_artifact_path(deplib), path)}
  end

  def get_source_files_for_dependency(deplib)
    (deplib[:artifacts][:source] || []).map {|path| File.join(get_artifact_path(deplib), path)}
  end

  def get_include_directories_for_dependency(deplib)
    paths = (deplib[:artifacts][:includes] || []).map {|path| File.join(get_artifact_path(deplib), path)}
    @ceedling[:file_system_utils].collect_paths(paths)
  end

  def set_env_if_required(lib_path)
    blob = @dependencies[lib_path]
    raise "Could not find dependency '#{lib_path}'" if blob.nil?
    return if (blob[:environment].nil?)
    return if (blob[:environment].empty?)

    blob[:environment].each do |e|
      m = e.match(/^(\w+)\s*(\+?\-?=)\s*(.*)$/)
      unless m.nil?
        case m[2]
        when "+="
          ENV[m[1]] = (ENV[m[1]] || "") + m[3]
        when "-="
          ENV[m[1]] = (ENV[m[1]] || "").gsub(m[3],'')
        else
          ENV[m[1]] = m[3]
        end
      end
    end
  end

  def fetch_if_required(lib_path)
    blob = @dependencies[lib_path]
    raise "Could not find dependency '#{lib_path}'" if blob.nil?
    return if (blob[:fetch].nil?)
    return if (blob[:fetch][:method].nil?)
    return if (directory(blob[:source_path]) && !Dir.empty?(blob[:source_path]))

    steps = case blob[:fetch][:method]
            when :none
              return
            when :zip
              [ "gzip -d #{blob[:fetch][:source]}" ]
            when :git
              branch = blob[:fetch][:tag] || blob[:fetch][:branch] || ''
              branch = ("-b " + branch) unless branch.empty?
              unless blob[:fetch][:hash].nil?
                # Do a deep clone to ensure the commit we want is available
                retval = [ "git clone #{branch} #{blob[:fetch][:source]} ." ]
                # Checkout the specified commit
                retval << "git checkout #{blob[:fetch][:hash]}"
              else
                # Do a thin clone
                retval = [ "git clone #{branch} --depth 1 #{blob[:fetch][:source]} ." ]
              end
            when :svn
              revision = blob[:fetch][:revision] || ''
              revision = ("--revision " + branch) unless branch.empty?
              retval = [ "svn checkout #{revision} #{blob[:fetch][:source]} ." ]
              retval
            when :custom
              blob[:fetch][:executable]
            else
              raise "Unknown fetch method '#{blob[:fetch][:method].to_s}' for dependency '#{blob[:name]}'"
            end

    # Perform the actual fetching
    @ceedling[:streaminator].stdout_puts("Fetching dependency #{blob[:name]}...", Verbosity::NORMAL)
    Dir.chdir(get_source_path(blob)) do
      steps.each do |step|
        @ceedling[:tool_executor].exec( step )
      end
    end
  end

  def build_if_required(lib_path)
    blob = @dependencies[lib_path]
    raise "Could not find dependency '#{lib_path}'" if blob.nil?

    # We don't clean anything unless we know how to fetch a new copy
    if (blob[:build].nil? || blob[:build].empty?)
      @ceedling[:streaminator].stdout_puts("Nothing to build for dependency #{blob[:name]}", Verbosity::NORMAL)
      return
    end

    # Perform the build
    @ceedling[:streaminator].stdout_puts("Building dependency #{blob[:name]}...", Verbosity::NORMAL)
    Dir.chdir(get_build_path(blob)) do
      blob[:build].each do |step|
        @ceedling[:tool_executor].exec( step )
      end
    end
  end

  def clean_if_required(lib_path)
    blob = @dependencies[lib_path]
    raise "Could not find dependency '#{lib_path}'" if blob.nil?

    # We don't clean anything unless we know how to fetch a new copy
    if (blob[:fetch].nil? || blob[:fetch][:method].nil? || (blob[:fetch][:method] == :none))
      @ceedling[:streaminator].stdout_puts("Nothing to clean for dependency #{blob[:name]}", Verbosity::NORMAL)
      return
    end

    # Perform the actual Cleaning
    @ceedling[:streaminator].stdout_puts("Cleaning dependency #{blob[:name]}...", Verbosity::NORMAL)
    get_working_paths(blob).each do |path|
      FileUtils.rm_rf(path) if File.directory?(path)
    end
  end

  def deploy_if_required(lib_path)
    blob = @dependencies[lib_path]
    raise "Could not find dependency '#{lib_path}'" if blob.nil?

    # We don't need to deploy anything if there isn't anything to deploy
    if (blob[:artifacts].nil? || blob[:artifacts][:dynamic_libraries].nil?  || blob[:artifacts][:dynamic_libraries].empty?)
      @ceedling[:streaminator].stdout_puts("Nothing to deploy for dependency #{blob[:name]}", Verbosity::NORMAL)
      return
    end

    # Perform the actual Deploying
    @ceedling[:streaminator].stdout_puts("Deploying dependency #{blob[:name]}...", Verbosity::NORMAL)
    FileUtils.cp( lib_path, File.dirname(PROJECT_RELEASE_BUILD_TARGET) )
  end

  def add_headers_and_sources()
    # Search for header file paths and files to add to our collections
    DEPENDENCIES_LIBRARIES.each do |deplib|
      get_include_directories_for_dependency(deplib).each do |header|
        cfg = @ceedling[:configurator].project_config_hash
        cfg[:collection_paths_include] << header
        cfg[:collection_paths_source_and_include] << header
        cfg[:collection_paths_test_support_source_include] << header
        cfg[:collection_paths_test_support_source_include_vendor] << header
        cfg[:collection_paths_release_toolchain_include] << header
        Dir[ File.join(header, "*#{EXTENSION_HEADER}") ].each do |f|
          cfg[:collection_all_headers] << f
        end
      end

      get_source_files_for_dependency(deplib).each do |source|
        cfg = @ceedling[:configurator].project_config_hash
        cfg[:collection_paths_source_and_include] << source
        cfg[:collection_paths_test_support_source_include] << source
        cfg[:collection_paths_test_support_source_include_vendor] << source
        cfg[:collection_paths_release_toolchain_include] << source
        Dir[ File.join(source, "*#{EXTENSION_SOURCE}") ].each do |f|
          cfg[:collection_all_source] << f
        end
      end
    end

    # Make all these updated files findable by Ceedling
    @ceedling[:file_finder].prepare_search_sources()
  end
end

# end blocks always executed following rake run
END {
}
