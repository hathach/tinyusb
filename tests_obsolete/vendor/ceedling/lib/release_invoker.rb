require 'constants'


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

end
