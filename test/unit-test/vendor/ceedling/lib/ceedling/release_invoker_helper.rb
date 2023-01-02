

class ReleaseInvokerHelper

  constructor :configurator, :dependinator, :task_invoker


  def process_deep_dependencies(dependencies_list)
    return if (not @configurator.project_use_deep_dependencies)

    if @configurator.project_generate_deep_dependencies
      @dependinator.enhance_release_file_dependencies( dependencies_list )
      @task_invoker.invoke_release_dependencies_files( dependencies_list )
    end

    @dependinator.load_release_object_deep_dependencies( dependencies_list )
  end

end
