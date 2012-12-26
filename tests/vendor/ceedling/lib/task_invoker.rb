require "par_map"

class TaskInvoker

  constructor :dependinator, :rake_utils, :rake_wrapper

  def setup
    @test_regexs = [/^#{TEST_ROOT_NAME}:/]
    @release_regexs = [/^#{RELEASE_ROOT_NAME}(:|$)/]
  end
  
  def add_test_task_regex(regex)
    @test_regexs << regex
  end

  def add_release_task_regex(regex)
    @release_regexs << regex
  end
  
  def test_invoked?
    invoked = false
    
    @test_regexs.each do |regex|
      invoked = true if (@rake_utils.task_invoked?(regex))
      break if invoked
    end
    
    return invoked
  end
  
  def release_invoked?
    invoked = false
    
    @release_regexs.each do |regex|
      invoked = true if (@rake_utils.task_invoked?(regex))
      break if invoked
    end
    
    return invoked
  end

  def invoked?(regex)
    return @rake_utils.task_invoked?(regex)
  end

  
  def invoke_test_mocks(mocks)
    @dependinator.enhance_mock_dependencies( mocks )
    mocks.each { |mock| @rake_wrapper[mock].invoke }
  end
  
  def invoke_test_runner(runner)
    @dependinator.enhance_runner_dependencies( runner )
    @rake_wrapper[runner].invoke
  end

  def invoke_test_shallow_include_lists(files)
    @dependinator.enhance_shallow_include_lists_dependencies( files )
    files.each { |file| @rake_wrapper[file].invoke }
  end

  def invoke_test_preprocessed_files(files)
    @dependinator.enhance_preprocesed_file_dependencies( files )
    files.each { |file| @rake_wrapper[file].invoke }
  end

  def invoke_test_dependencies_files(files)
    @dependinator.enhance_dependencies_dependencies( files )
    files.each { |file| @rake_wrapper[file].invoke }
  end

  def invoke_test_results(result)
    @dependinator.enhance_results_dependencies( result )
    @rake_wrapper[result].invoke
  end

  def invoke_release_dependencies_files(files)
    par_map(PROJECT_COMPILE_THREADS, files) do |file|
       @rake_wrapper[file].invoke
    end
  end
  
  def invoke_release_objects(objects)
    par_map(PROJECT_COMPILE_THREADS, objects) do |object|
       @rake_wrapper[object].invoke
    end
  end
  
end
