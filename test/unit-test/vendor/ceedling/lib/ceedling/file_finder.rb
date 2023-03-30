require 'rubygems'
require 'rake' # for adding ext() method to string
require 'thread'


class FileFinder
  SEMAPHORE = Mutex.new

  constructor :configurator, :file_finder_helper, :cacheinator, :file_path_utils, :file_wrapper, :yaml_wrapper

  def prepare_search_sources
    @all_test_source_and_header_file_collection =
      @configurator.collection_all_tests +
      @configurator.collection_all_source +
      @configurator.collection_all_headers
  end


  def find_header_file(mock_file)
    header = File.basename(mock_file).sub(/#{@configurator.cmock_mock_prefix}/, '').ext(@configurator.extension_header)

    found_path = @file_finder_helper.find_file_in_collection(header, @configurator.collection_all_headers, :error)

    return found_path
  end


  def find_header_input_for_mock_file(mock_file)
    found_path = find_header_file(mock_file)
    mock_input = found_path

    if (@configurator.project_use_test_preprocessor)
      mock_input = @cacheinator.diff_cached_test_file( @file_path_utils.form_preprocessed_file_filepath( found_path ) )
    end

    return mock_input
  end


  def find_source_from_test(test, complain)
    test_prefix  = @configurator.project_test_file_prefix
    source_paths = @configurator.collection_all_source

    source = File.basename(test).sub(/#{test_prefix}/, '')

    # we don't blow up if a test file has no corresponding source file
    return @file_finder_helper.find_file_in_collection(source, source_paths, complain)
  end


  def find_test_from_runner_path(runner_path)
    extension_source = @configurator.extension_source

    test_file = File.basename(runner_path).sub(/#{@configurator.test_runner_file_suffix}#{'\\'+extension_source}/, extension_source)

    found_path = @file_finder_helper.find_file_in_collection(test_file, @configurator.collection_all_tests, :error)

    return found_path
  end


  def find_test_input_for_runner_file(runner_path)
    found_path   = find_test_from_runner_path(runner_path)
    runner_input = found_path

    if (@configurator.project_use_test_preprocessor)
      runner_input = @cacheinator.diff_cached_test_file( @file_path_utils.form_preprocessed_file_filepath( found_path ) )
    end

    return runner_input
  end


  def find_test_from_file_path(file_path)
    test_file = File.basename(file_path).ext(@configurator.extension_source)

    found_path = @file_finder_helper.find_file_in_collection(test_file, @configurator.collection_all_tests, :error)

    return found_path
  end


  def find_test_or_source_or_header_file(file_path)
    file = File.basename(file_path)
    return @file_finder_helper.find_file_in_collection(file, @all_test_source_and_header_file_collection, :error)
  end


  def find_compilation_input_file(file_path, complain=:error, release=false)
    found_file = nil

    source_file = File.basename(file_path).ext(@configurator.extension_source)

    # We only collect files that already exist when we start up.
    # FileLists can produce undesired results for dynamically generated files depending on when they're accessed.
    # So collect mocks and runners separately and right now.

    SEMAPHORE.synchronize {

      if (source_file =~ /#{@configurator.test_runner_file_suffix}/)
        found_file =
          @file_finder_helper.find_file_in_collection(
            source_file,
            @file_wrapper.directory_listing( File.join(@configurator.project_test_runners_path, '*') ),
            complain)

      elsif (@configurator.project_use_mocks and (source_file =~ /#{@configurator.cmock_mock_prefix}/))
        found_file =
          @file_finder_helper.find_file_in_collection(
            source_file,
            @file_wrapper.directory_listing( File.join(@configurator.cmock_mock_path, '*') ),
            complain)

      elsif release
        found_file =
          @file_finder_helper.find_file_in_collection(
            source_file,
            @configurator.collection_release_existing_compilation_input,
            complain)
      else
        temp_complain = (defined?(TEST_BUILD_USE_ASSEMBLY) && TEST_BUILD_USE_ASSEMBLY) ? :ignore : complain
        found_file =
          @file_finder_helper.find_file_in_collection(
            source_file,
            @configurator.collection_all_existing_compilation_input,
            temp_complain)
        found_file ||= find_assembly_file(file_path, false) if (defined?(TEST_BUILD_USE_ASSEMBLY) && TEST_BUILD_USE_ASSEMBLY)
      end
    }
    return found_file
  end


  def find_source_file(file_path, complain)
    source_file = File.basename(file_path).ext(@configurator.extension_source)
    return @file_finder_helper.find_file_in_collection(source_file, @configurator.collection_all_source, complain)
  end


  def find_assembly_file(file_path, complain = :error)
    assembly_file = File.basename(file_path).ext(@configurator.extension_assembly)
    return @file_finder_helper.find_file_in_collection(assembly_file, @configurator.collection_all_assembly, complain)
  end

  def find_file_from_list(file_path, file_list, complain)
    return @file_finder_helper.find_file_in_collection(file_path, file_list, complain)
  end
end
