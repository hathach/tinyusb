

rule(/#{PROJECT_RELEASE_DEPENDENCIES_PATH}\/#{'.+\\'+EXTENSION_DEPENDENCIES}$/ => [
    proc do |task_name|
      @ceedling[:file_finder].find_compilation_input_file(task_name, :error, true)
    end  
  ]) do |dep|
  @ceedling[:generator].generate_dependencies_file(
  	TOOLS_RELEASE_DEPENDENCIES_GENERATOR,
  	RELEASE_SYM,
  	dep.source,
  	@ceedling[:file_path_utils].form_release_build_c_object_filepath(dep.source),
  	dep.name)
end

