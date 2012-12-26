
class Preprocessinator

  attr_reader :preprocess_file_proc
  
  constructor :preprocessinator_helper, :preprocessinator_includes_handler, :preprocessinator_file_handler, :task_invoker, :file_path_utils, :yaml_wrapper


  def setup
    # fashion ourselves callbacks @preprocessinator_helper can use
    @preprocess_includes_proc = Proc.new { |filepath| self.preprocess_shallow_includes(filepath) }
    @preprocess_file_proc     = Proc.new { |filepath| self.preprocess_file(filepath) }
  end


  def preprocess_test_and_invoke_test_mocks(test)
    @preprocessinator_helper.preprocess_includes(test, @preprocess_includes_proc)

    mocks_list = @preprocessinator_helper.assemble_mocks_list(test)

    @preprocessinator_helper.preprocess_mockable_headers(mocks_list, @preprocess_file_proc)

    @task_invoker.invoke_test_mocks(mocks_list)

    @preprocessinator_helper.preprocess_test_file(test, @preprocess_file_proc)
    
    return mocks_list
  end

  def preprocess_shallow_includes(filepath)
    dependencies_rule = @preprocessinator_includes_handler.form_shallow_dependencies_rule(filepath)
    includes          = @preprocessinator_includes_handler.extract_shallow_includes(dependencies_rule)

    @preprocessinator_includes_handler.write_shallow_includes_list(
      @file_path_utils.form_preprocessed_includes_list_filepath(filepath), includes)
  end

  def preprocess_file(filepath)
    @preprocessinator_includes_handler.invoke_shallow_includes_list(filepath)
    @preprocessinator_file_handler.preprocess_file( filepath, @yaml_wrapper.load(@file_path_utils.form_preprocessed_includes_list_filepath(filepath)) )
  end

end
