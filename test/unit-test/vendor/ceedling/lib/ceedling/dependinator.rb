
class Dependinator

  constructor :configurator, :project_config_manager, :test_includes_extractor, :file_path_utils, :rake_wrapper, :file_wrapper

  def touch_force_rebuild_files
    @file_wrapper.touch( @configurator.project_test_force_rebuild_filepath )
    @file_wrapper.touch( @configurator.project_release_force_rebuild_filepath ) if (@configurator.project_release_build)
  end



  def load_release_object_deep_dependencies(dependencies_list)
    dependencies_list.each do |dependencies_file|
      if File.exists?(dependencies_file)
        @rake_wrapper.load_dependencies( dependencies_file )
      end
    end
  end


  def enhance_release_file_dependencies(files)
    files.each do |filepath|
      @rake_wrapper[filepath].enhance( [@configurator.project_release_force_rebuild_filepath] ) if (@project_config_manager.release_config_changed)
    end
  end



  def load_test_object_deep_dependencies(files_list)
    dependencies_list = @file_path_utils.form_test_dependencies_filelist(files_list)
    dependencies_list.each do |dependencies_file|
      if File.exists?(dependencies_file)
        @rake_wrapper.load_dependencies(dependencies_file)
      end
    end
  end


  def enhance_runner_dependencies(runner_filepath)
    @rake_wrapper[runner_filepath].enhance( [@configurator.project_test_force_rebuild_filepath] ) if (@project_config_manager.test_config_changed ||
      @project_config_manager.test_defines_changed)
  end


  def enhance_shallow_include_lists_dependencies(include_lists)
    include_lists.each do |include_list_filepath|
      @rake_wrapper[include_list_filepath].enhance( [@configurator.project_test_force_rebuild_filepath] ) if (@project_config_manager.test_config_changed ||
        @project_config_manager.test_defines_changed)
    end
  end


  def enhance_preprocesed_file_dependencies(files)
    files.each do |filepath|
      @rake_wrapper[filepath].enhance( [@configurator.project_test_force_rebuild_filepath] ) if (@project_config_manager.test_config_changed ||
        @project_config_manager.test_defines_changed)
    end
  end


  def enhance_mock_dependencies(mocks_list)
    # if input configuration or ceedling changes, make sure these guys get rebuilt
    mocks_list.each do |mock_filepath|
      @rake_wrapper[mock_filepath].enhance( [@configurator.project_test_force_rebuild_filepath] ) if (@project_config_manager.test_config_changed ||
        @project_config_manager.test_defines_changed)
      @rake_wrapper[mock_filepath].enhance( @configurator.cmock_unity_helper )                    if (@configurator.cmock_unity_helper)
    end
  end


  def enhance_dependencies_dependencies(dependencies)
    dependencies.each do |dependencies_filepath|
      @rake_wrapper[dependencies_filepath].enhance( [@configurator.project_test_force_rebuild_filepath] ) if (@project_config_manager.test_config_changed ||
        @project_config_manager.test_defines_changed)
    end
  end


  def enhance_test_build_object_dependencies(objects)
    objects.each do |object_filepath|
      @rake_wrapper[object_filepath].enhance( [@configurator.project_test_force_rebuild_filepath] ) if (@project_config_manager.test_config_changed ||
        @project_config_manager.test_defines_changed)
    end
  end


  def enhance_results_dependencies(result_filepath)
    @rake_wrapper[result_filepath].enhance( [@configurator.project_test_force_rebuild_filepath] ) if @project_config_manager.test_config_changed
  end


  def enhance_test_executable_dependencies(test, objects)
    @rake_wrapper[ @file_path_utils.form_test_executable_filepath(test) ].enhance( objects )
  end

end
