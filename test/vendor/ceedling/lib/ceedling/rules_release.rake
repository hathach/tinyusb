
RELEASE_COMPILE_TASK_ROOT  = RELEASE_TASK_ROOT + 'compile:'  unless defined?(RELEASE_COMPILE_TASK_ROOT)
RELEASE_ASSEMBLE_TASK_ROOT = RELEASE_TASK_ROOT + 'assemble:' unless defined?(RELEASE_ASSEMBLE_TASK_ROOT)

# If GCC and Releasing a Library, Update Tools to Automatically Have Necessary Tags
if (TOOLS_RELEASE_COMPILER[:executable] == DEFAULT_RELEASE_COMPILER_TOOL[:executable])
  if (File.extname(PROJECT_RELEASE_BUILD_TARGET) == '.so')
    TOOLS_RELEASE_COMPILER[:arguments] << "-fPIC" unless TOOLS_RELEASE_COMPILER[:arguments].include?("-fPIC")
    TOOLS_RELEASE_LINKER[:arguments] << "-shared" unless TOOLS_RELEASE_LINKER[:arguments].include?("-shared")
  elsif (File.extname(PROJECT_RELEASE_BUILD_TARGET) == '.a')
    TOOLS_RELEASE_COMPILER[:arguments] << "-fPIC" unless TOOLS_RELEASE_COMPILER[:arguments].include?("-fPIC")
    TOOLS_RELEASE_LINKER[:executable] = 'ar'
    TOOLS_RELEASE_LINKER[:arguments] = ['rcs', '${2}', '${1}'].compact
  end
end

if (RELEASE_BUILD_USE_ASSEMBLY)
rule(/#{PROJECT_RELEASE_BUILD_OUTPUT_ASM_PATH}\/#{'.+\\'+EXTENSION_OBJECT}$/ => [
    proc do |task_name|
      @ceedling[:file_finder].find_assembly_file(task_name)
    end
  ]) do |object|
  @ceedling[:generator].generate_object_file(
    TOOLS_RELEASE_ASSEMBLER,
    OPERATION_ASSEMBLE_SYM,
    RELEASE_SYM,
    object.source,
    object.name )
end
end


rule(/#{PROJECT_RELEASE_BUILD_OUTPUT_C_PATH}\/#{'.+\\'+EXTENSION_OBJECT}$/ => [
    proc do |task_name|
      @ceedling[:file_finder].find_compilation_input_file(task_name, :error, true)
    end
  ]) do |object|
  @ceedling[:generator].generate_object_file(
    TOOLS_RELEASE_COMPILER,
    OPERATION_COMPILE_SYM,
    RELEASE_SYM,
    object.source,
    object.name,
    @ceedling[:file_path_utils].form_release_build_c_list_filepath( object.name ),
    @ceedling[:file_path_utils].form_release_dependencies_filepath( object.name ) )
end


rule(/#{PROJECT_RELEASE_BUILD_TARGET}/) do |bin_file|
  objects, libraries = @ceedling[:release_invoker].sort_objects_and_libraries(bin_file.prerequisites)
  tool      = TOOLS_RELEASE_LINKER.clone
  lib_args  = @ceedling[:release_invoker].convert_libraries_to_arguments(libraries)
  lib_paths = @ceedling[:release_invoker].get_library_paths_to_arguments()
  map_file  = @ceedling[:configurator].project_release_build_map
  @ceedling[:generator].generate_executable_file(
    tool,
    RELEASE_SYM,
    objects,
    bin_file.name,
    map_file,
    lib_args,
    lib_paths )
  @ceedling[:release_invoker].artifactinate( bin_file.name, map_file, @ceedling[:configurator].release_build_artifacts )
end


namespace RELEASE_SYM do
  # use rules to increase efficiency for large projects (instead of iterating through all sources and creating defined tasks)

  namespace :compile do
    rule(/^#{RELEASE_COMPILE_TASK_ROOT}\S+#{'\\'+EXTENSION_SOURCE}$/ => [ # compile task names by regex
        proc do |task_name|
          source = task_name.sub(/#{RELEASE_COMPILE_TASK_ROOT}/, '')
          @ceedling[:file_finder].find_source_file(source, :error)
        end
    ]) do |compile|
      @ceedling[:rake_wrapper][:directories].invoke
      @ceedling[:project_config_manager].process_release_config_change
      @ceedling[:release_invoker].setup_and_invoke_c_objects( [compile.source] )
    end
  end

  if (RELEASE_BUILD_USE_ASSEMBLY)
  namespace :assemble do
    rule(/^#{RELEASE_ASSEMBLE_TASK_ROOT}\S+#{'\\'+EXTENSION_ASSEMBLY}$/ => [ # assemble task names by regex
        proc do |task_name|
          source = task_name.sub(/#{RELEASE_ASSEMBLE_TASK_ROOT}/, '')
          @ceedling[:file_finder].find_assembly_file(source)
        end
    ]) do |assemble|
      @ceedling[:rake_wrapper][:directories].invoke
      @ceedling[:project_config_manager].process_release_config_change
      @ceedling[:release_invoker].setup_and_invoke_asm_objects( [assemble.source] )
    end
  end
  end

end

