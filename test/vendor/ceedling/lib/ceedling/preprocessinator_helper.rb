

class PreprocessinatorHelper

  constructor :configurator, :test_includes_extractor, :task_invoker, :file_finder, :file_path_utils


  def preprocess_includes(test, preprocess_includes_proc)
    if (@configurator.project_use_test_preprocessor)
      preprocessed_includes_list = @file_path_utils.form_preprocessed_includes_list_filepath(test)
      preprocess_includes_proc.call( @file_finder.find_test_from_file_path(preprocessed_includes_list) )
      @test_includes_extractor.parse_includes_list(preprocessed_includes_list)
    else
      @test_includes_extractor.parse_test_file(test)
    end
  end

  def assemble_mocks_list(test)
    return @file_path_utils.form_mocks_source_filelist( @test_includes_extractor.lookup_raw_mock_list(test) )
  end

  def preprocess_mockable_headers(mock_list, preprocess_file_proc)
    if (@configurator.project_use_test_preprocessor)
      preprocess_files_smartly(
        @file_path_utils.form_preprocessed_mockable_headers_filelist(mock_list),
        preprocess_file_proc ) { |file| @file_finder.find_header_file(file) }
    end
  end

  def preprocess_test_file(test, preprocess_file_proc)
    return if (!@configurator.project_use_test_preprocessor)

    preprocess_file_proc.call(test)
  end

  private ############################

  def preprocess_files_smartly(file_list, preprocess_file_proc)
    if (@configurator.project_use_deep_dependencies)
      @task_invoker.invoke_test_preprocessed_files(file_list)
    else
      file_list.each { |file| preprocess_file_proc.call( yield(file) ) }
    end
  end

end
