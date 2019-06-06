require 'rubygems'
require 'rake' # for ext()
require 'fileutils'
require 'system_wrapper'

# global utility methods (for plugins, project files, etc.)
def ceedling_form_filepath(destination_path, original_filepath, new_extension=nil)
  filename = File.basename(original_filepath)
  filename.replace(filename.ext(new_extension)) if (!new_extension.nil?)
  return File.join( destination_path.gsub(/\\/, '/'), filename )
end

class FilePathUtils

  GLOB_MATCHER = /[\*\?\{\}\[\]]/

  constructor :configurator, :file_wrapper


  ######### class methods ##########

  # standardize path to use '/' path separator & begin with './' & have no trailing path separator
  def self.standardize(path)
    path.strip!
    path.gsub!(/\\/, '/')
    path.gsub!(/^((\+|-):)?\.\//, '')
    path.chomp!('/')
    return path
  end

  def self.os_executable_ext(executable)
    return executable.ext('.exe') if SystemWrapper.windows?
    return executable
  end

  # extract directory path from between optional add/subtract aggregation modifiers and up to glob specifiers
  # note: slightly different than File.dirname in that /files/foo remains /files/foo and does not become /files
  def self.extract_path(path)
    path = path.sub(/^(\+|-):/, '')
    
    # find first occurrence of path separator followed by directory glob specifier: *, ?, {, }, [, ]
    find_index = (path =~ GLOB_MATCHER)
    
    # no changes needed (lop off final path separator)
    return path.chomp('/') if (find_index.nil?)
    
    # extract up to first glob specifier
    path = path[0..(find_index-1)]
    
    # lop off everything up to and including final path separator
    find_index = path.rindex('/')
    return path[0..(find_index-1)] if (not find_index.nil?)
    
    # return string up to first glob specifier if no path separator found
    return path
  end

  # return whether the given path is to be aggregated (no aggregation modifier defaults to same as +:)
  def self.add_path?(path)
    return (path =~ /^-:/).nil?
  end
  
  # get path (and glob) lopping off optional +: / -: prefixed aggregation modifiers
  def self.extract_path_no_aggregation_operators(path)
    return path.sub(/^(\+|-):/, '')
  end
  
  # all the globs that may be in a path string work fine with one exception;
  # to recurse through all subdirectories, the glob is dir/**/** but our paths use
  # convention of only dir/**
  def self.reform_glob(path)
    return path if (path =~ /\/\*\*$/).nil?
    return path + '/**'
  end

  def self.form_ceedling_vendor_path(*filepaths)
    return File.join( CEEDLING_VENDOR, filepaths )
  end

  ######### instance methods ##########

  def form_temp_path(filepath, prefix='')
    return File.join( @configurator.project_temp_path, prefix + File.basename(filepath) )    
  end
  
  ### release ###
  def form_release_build_cache_path(filepath)
    return File.join( @configurator.project_release_build_cache_path, File.basename(filepath) )    
  end
  
  def form_release_dependencies_filepath(filepath)
    return File.join( @configurator.project_release_dependencies_path, File.basename(filepath).ext(@configurator.extension_dependencies) )
  end
  
  def form_release_build_c_object_filepath(filepath)
    return File.join( @configurator.project_release_build_output_c_path, File.basename(filepath).ext(@configurator.extension_object) )
  end

  def form_release_build_asm_object_filepath(filepath)
    return File.join( @configurator.project_release_build_output_asm_path, File.basename(filepath).ext(@configurator.extension_object) )
  end

  def form_release_build_c_objects_filelist(files)
    return (@file_wrapper.instantiate_file_list(files)).pathmap("#{@configurator.project_release_build_output_c_path}/%n#{@configurator.extension_object}")
  end

  def form_release_build_asm_objects_filelist(files)
    return (@file_wrapper.instantiate_file_list(files)).pathmap("#{@configurator.project_release_build_output_asm_path}/%n#{@configurator.extension_object}")
  end

  def form_release_build_c_list_filepath(filepath)
    return File.join( @configurator.project_release_build_output_c_path, File.basename(filepath).ext(@configurator.extension_list) )
  end
  
  def form_release_dependencies_filelist(files)
    return (@file_wrapper.instantiate_file_list(files)).pathmap("#{@configurator.project_release_dependencies_path}/%n#{@configurator.extension_dependencies}")
  end
  
  ### tests ###
  def form_test_build_cache_path(filepath)
    return File.join( @configurator.project_test_build_cache_path, File.basename(filepath) )    
  end
  
  def form_pass_results_filepath(filepath)
    return File.join( @configurator.project_test_results_path, File.basename(filepath).ext(@configurator.extension_testpass) )
  end

  def form_fail_results_filepath(filepath)
    return File.join( @configurator.project_test_results_path, File.basename(filepath).ext(@configurator.extension_testfail) )
  end

  def form_runner_filepath_from_test(filepath)
    return File.join( @configurator.project_test_runners_path, File.basename(filepath, @configurator.extension_source)) + @configurator.test_runner_file_suffix + @configurator.extension_source
  end

  def form_test_filepath_from_runner(filepath)
    return filepath.sub(/#{TEST_RUNNER_FILE_SUFFIX}/, '')
  end

  def form_runner_object_filepath_from_test(filepath)
    return (form_test_build_object_filepath(filepath)).sub(/(#{@configurator.extension_object})$/, "#{@configurator.test_runner_file_suffix}\\1")
  end

  def form_test_build_object_filepath(filepath)
    return File.join( @configurator.project_test_build_output_path, File.basename(filepath).ext(@configurator.extension_object) )
  end

  def form_test_executable_filepath(filepath)
    return File.join( @configurator.project_test_build_output_path, File.basename(filepath).ext(@configurator.extension_executable) )    
  end

  def form_test_build_map_filepath(filepath)
    return File.join( @configurator.project_test_build_output_path, File.basename(filepath).ext(@configurator.extension_map) )
  end

  def form_test_build_list_filepath(filepath)
    return File.join( @configurator.project_test_build_output_path, File.basename(filepath).ext(@configurator.extension_list) )
  end

  def form_preprocessed_file_filepath(filepath)
    return File.join( @configurator.project_test_preprocess_files_path, File.basename(filepath) )    
  end

  def form_preprocessed_includes_list_filepath(filepath)
    return File.join( @configurator.project_test_preprocess_includes_path, File.basename(filepath) )    
  end

  def form_test_build_objects_filelist(sources)
    return (@file_wrapper.instantiate_file_list(sources)).pathmap("#{@configurator.project_test_build_output_path}/%n#{@configurator.extension_object}")
  end
  
  def form_preprocessed_mockable_headers_filelist(mocks)
    # pathmapping note: "%{#{@configurator.cmock_mock_prefix},}n" replaces mock_prefix with nothing (signified by absence of anything after comma inside replacement brackets)
    return (@file_wrapper.instantiate_file_list(mocks)).pathmap("#{@configurator.project_test_preprocess_files_path}/%{#{@configurator.cmock_mock_prefix},}n#{@configurator.extension_header}")
  end

  def form_mocks_source_filelist(mocks)
    return (@file_wrapper.instantiate_file_list(mocks)).pathmap("#{@configurator.cmock_mock_path}/%n#{@configurator.extension_source}")
  end

  def form_test_dependencies_filelist(files)
    return (@file_wrapper.instantiate_file_list(files)).pathmap("#{@configurator.project_test_dependencies_path}/%n#{@configurator.extension_dependencies}")    
  end

  def form_pass_results_filelist(path, files)
    return (@file_wrapper.instantiate_file_list(files)).pathmap("#{path}/%n#{@configurator.extension_testpass}")    
  end

end
