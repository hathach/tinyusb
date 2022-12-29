require 'ceedling/constants'


class ReleaseInvoker

  constructor :configurator, :release_invoker_helper, :build_invoker_utils, :dependinator, :task_invoker, :file_path_utils, :file_wrapper


  def setup_and_invoke_c_objects( c_files )
    objects = @file_path_utils.form_release_build_c_objects_filelist( c_files )

    begin
      @release_invoker_helper.process_deep_dependencies( @file_path_utils.form_release_dependencies_filelist( c_files ) )

      @dependinator.enhance_release_file_dependencies( objects )
      @task_invoker.invoke_release_objects( objects )
    rescue => e
      @build_invoker_utils.process_exception( e, RELEASE_SYM, false )
    end

    return objects
  end


  def setup_and_invoke_asm_objects( asm_files )
    objects = @file_path_utils.form_release_build_asm_objects_filelist( asm_files )

    begin
      @dependinator.enhance_release_file_dependencies( objects )
      @task_invoker.invoke_release_objects( objects )
    rescue => e
      @build_invoker_utils.process_exception( e, RELEASE_SYM, false )
    end

    return objects
  end


  def refresh_c_deep_dependencies
    return if (not @configurator.project_use_deep_dependencies)

    @file_wrapper.rm_f(
      @file_wrapper.directory_listing(
        File.join( @configurator.project_release_dependencies_path, '*' + @configurator.extension_dependencies ) ) )

    @release_invoker_helper.process_deep_dependencies(
      @file_path_utils.form_release_dependencies_filelist(
        @configurator.collection_all_source ) )
  end


  def artifactinate( *files )
    files.flatten.each do |file|
      @file_wrapper.cp( file, @configurator.project_release_artifacts_path ) if @file_wrapper.exist?( file )
    end
  end

  def convert_libraries_to_arguments(libraries)
    args = ((libraries || []) + ((defined? LIBRARIES_SYSTEM) ? LIBRARIES_SYSTEM : [])).flatten
    if (defined? LIBRARIES_FLAG)
      args.map! {|v| LIBRARIES_FLAG.gsub(/\$\{1\}/, v) }
    end
    return args
  end

  def get_library_paths_to_arguments()
    paths = (defined? PATHS_LIBRARIES) ? (PATHS_LIBRARIES || []).clone : []
    if (defined? LIBRARIES_PATH_FLAG)
      paths.map! {|v| LIBRARIES_PATH_FLAG.gsub(/\$\{1\}/, v) }
    end
    return paths
  end

  def sort_objects_and_libraries(both)
    extension = if ((defined? EXTENSION_SUBPROJECTS) && (defined? EXTENSION_LIBRARIES))
      extension_libraries = if (EXTENSION_LIBRARIES.class == Array)
                              EXTENSION_LIBRARIES.join(")|(?:\\")
                            else
                              EXTENSION_LIBRARIES
                            end
      "(?:\\#{EXTENSION_SUBPROJECTS})|(?:\\#{extension_libraries})"
    elsif (defined? EXTENSION_SUBPROJECTS)
      "\\#{EXTENSION_SUBPROJECTS}"
    elsif (defined? EXTENSION_LIBRARIES)
      if (EXTENSION_LIBRARIES.class == Array)
        "(?:\\#{EXTENSION_LIBRARIES.join(")|(?:\\")})"
      else
        "\\#{EXTENSION_LIBRARIES}"
      end
    else
      "\\.LIBRARY"
    end
    sorted_objects = both.group_by {|v| v.match(/.+#{extension}$/) ? :libraries : :objects }
    libraries = sorted_objects[:libraries] || []
    objects   = sorted_objects[:objects]   || []
    return objects, libraries
  end
end
