
class Preprocessinator

  constructor :preprocessinator_helper, :preprocessinator_includes_handler, :preprocessinator_file_handler, :task_invoker, :file_path_utils, :yaml_wrapper, :project_config_manager, :configurator


  def setup
    # fashion ourselves callbacks @preprocessinator_helper can use
    @preprocess_includes_proc  = Proc.new { |filepath| self.preprocess_shallow_includes(filepath) }
    @preprocess_mock_file_proc = Proc.new { |filepath| self.preprocess_file(filepath) }
    @preprocess_test_file_directives_proc = Proc.new { |filepath| self.preprocess_file_directives(filepath) }
    @preprocess_test_file_proc = Proc.new { |filepath| self.preprocess_file(filepath) }
  end

  def preprocess_shallow_source_includes(test)
    @preprocessinator_helper.preprocess_source_includes(test)
  end

  def preprocess_test_and_invoke_test_mocks(test)
    @preprocessinator_helper.preprocess_includes(test, @preprocess_includes_proc)

    mocks_list = @preprocessinator_helper.assemble_mocks_list(test)

    @project_config_manager.process_test_defines_change(mocks_list)

    @preprocessinator_helper.preprocess_mockable_headers(mocks_list, @preprocess_mock_file_proc)

    @task_invoker.invoke_test_mocks(mocks_list)

    if (@configurator.project_use_preprocessor_directives)
      @preprocessinator_helper.preprocess_test_file(test, @preprocess_test_file_directives_proc)
    else
      @preprocessinator_helper.preprocess_test_file(test, @preprocess_test_file_proc)
    end

    return mocks_list
  end

  def preprocess_shallow_includes(filepath)
    includes = @preprocessinator_includes_handler.extract_includes(filepath)

    @preprocessinator_includes_handler.write_shallow_includes_list(
      @file_path_utils.form_preprocessed_includes_list_filepath(filepath), includes)
  end

  def preprocess_file(filepath)
    @preprocessinator_includes_handler.invoke_shallow_includes_list(filepath)
    @preprocessinator_file_handler.preprocess_file( filepath, @yaml_wrapper.load(@file_path_utils.form_preprocessed_includes_list_filepath(filepath)) )
  end

  def preprocess_file_directives(filepath)
    @preprocessinator_includes_handler.invoke_shallow_includes_list( filepath )
    @preprocessinator_file_handler.preprocess_file_directives( filepath,
      @yaml_wrapper.load( @file_path_utils.form_preprocessed_includes_list_filepath( filepath ) ) )
  end
end
