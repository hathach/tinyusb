
class TestInvokerHelper

  constructor :configurator, :task_invoker, :test_includes_extractor, :file_finder, :file_path_utils, :file_wrapper

  def clean_results(results, options)
    @file_wrapper.rm_f( results[:fail] )
    @file_wrapper.rm_f( results[:pass] ) if (options[:force_run])
  end

  def process_deep_dependencies(files)
    return if (not @configurator.project_use_deep_dependencies)

    dependencies_list = @file_path_utils.form_test_dependencies_filelist( files )

    if @configurator.project_generate_deep_dependencies
      @task_invoker.invoke_test_dependencies_files( dependencies_list )
    end

    yield( dependencies_list ) if block_given?
  end
  
  def extract_sources(test)
    sources  = []
    includes = @test_includes_extractor.lookup_includes_list(test)
    
    includes.each { |include| sources << @file_finder.find_compilation_input_file(include, :ignore) }
    
    return sources.compact
  end
  
end
