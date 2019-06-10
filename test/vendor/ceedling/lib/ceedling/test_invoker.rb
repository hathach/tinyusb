require 'ceedling/constants'


class TestInvoker

  attr_reader :sources, :tests, :mocks

  constructor :configurator,
              :test_invoker_helper,
              :plugin_manager,
              :streaminator,
              :preprocessinator,
              :task_invoker,
              :dependinator,
              :project_config_manager,
              :build_invoker_utils,
              :file_path_utils,
              :file_wrapper

  def setup
    @sources = []
    @tests   = []
    @mocks   = []
  end

  def get_test_definition_str(test)
    return "-D" + File.basename(test, File.extname(test)).upcase.sub(/@.*$/, "")
  end

  def get_tools_compilers
    tools_compilers = Hash.new
    tools_compilers["for unit test"] = TOOLS_TEST_COMPILER if defined? TOOLS_TEST_COMPILER
    tools_compilers["for gcov"]      = TOOLS_GCOV_COMPILER if defined? TOOLS_GCOV_COMPILER
    return tools_compilers
  end

  def add_test_definition(test)
    test_definition_str = get_test_definition_str(test)
    get_tools_compilers.each do |tools_compiler_key, tools_compiler_value|
      tools_compiler_value[:arguments].push("-D#{File.basename(test, ".*").strip.upcase.sub(/@.*$/, "")}")
      @streaminator.stdout_puts("Add the definition value in the build option #{tools_compiler_value[:arguments][-1]} #{tools_compiler_key}", Verbosity::OBNOXIOUS)
    end
  end

  def delete_test_definition(test)
    test_definition_str = get_test_definition_str(test)
    get_tools_compilers.each do |tools_compiler_key, tools_compiler_value|
      num_options = tools_compiler_value[:arguments].size
      @streaminator.stdout_puts("Delete the definition value in the build option #{tools_compiler_value[:arguments][-1]} #{tools_compiler_key}", Verbosity::OBNOXIOUS)
      tools_compiler_value[:arguments].delete_if{|i| i == test_definition_str}
      if num_options > tools_compiler_value[:arguments].size + 1
        @streaminator.stderr_puts("WARNING: duplicated test definition.")
      end
    end
  end

  def setup_and_invoke(tests, context=TEST_SYM, options={:force_run => true})

    @tests = tests

    @project_config_manager.process_test_config_change

    @tests.each do |test|
      # announce beginning of test run
      header = "Test '#{File.basename(test)}'"
      @streaminator.stdout_puts("\n\n#{header}\n#{'-' * header.length}")

      begin
        @plugin_manager.pre_test( test )
        test_name ="#{File.basename(test)}".chomp('.c')
        def_test_key="defines_#{test_name}"

        # Re-define the project out path and pre-processor defines.
        if @configurator.project_config_hash.has_key?(def_test_key.to_sym)
          @project_config_manager.test_config_changed
          defs_bkp = Array.new(COLLECTION_DEFINES_TEST_AND_VENDOR)
          printf " ************** Specific test definitions for #{test_name} !!! \n"
          tst_defs_cfg = @configurator.project_config_hash[def_test_key.to_sym]

          orig_path = @configurator.project_test_build_output_path
          @configurator.project_config_hash[:project_test_build_output_path] = File.join(@configurator.project_test_build_output_path, test_name)
          @file_wrapper.mkdir(@configurator.project_test_build_output_path)
          COLLECTION_DEFINES_TEST_AND_VENDOR.replace(tst_defs_cfg)
          # printf " *  new defines = #{COLLECTION_DEFINES_TEST_AND_VENDOR}\n"
        end

        # collect up test fixture pieces & parts
        runner       = @file_path_utils.form_runner_filepath_from_test( test )
        mock_list    = @preprocessinator.preprocess_test_and_invoke_test_mocks( test )
        sources      = @test_invoker_helper.extract_sources( test )
        extras       = @configurator.collection_test_fixture_extra_link_objects
        core         = [test] + mock_list + sources
        objects      = @file_path_utils.form_test_build_objects_filelist( [runner] + core + extras )
        results_pass = @file_path_utils.form_pass_results_filepath( test )
        results_fail = @file_path_utils.form_fail_results_filepath( test )

        # add the definition value in the build option for the unit test
        if @configurator.defines_use_test_definition
          add_test_definition(test)
        end

        # clean results files so we have a missing file with which to kick off rake's dependency rules
        @test_invoker_helper.clean_results( {:pass => results_pass, :fail => results_fail}, options )

        # load up auxiliary dependencies so deep changes cause rebuilding appropriately
        @test_invoker_helper.process_deep_dependencies( core ) do |dependencies_list|
          @dependinator.load_test_object_deep_dependencies( dependencies_list )
        end

        # tell rake to create test runner if needed
        @task_invoker.invoke_test_runner( runner )

        # enhance object file dependencies to capture externalities influencing regeneration
        @dependinator.enhance_test_build_object_dependencies( objects )

        # associate object files with executable
        @dependinator.setup_test_executable_dependencies( test, objects )

        # build test objects
        @task_invoker.invoke_test_objects( objects )

        # 3, 2, 1... launch
        @task_invoker.invoke_test_results( results_pass )
      rescue => e
        @build_invoker_utils.process_exception( e, context )
      ensure
        # delete the definition value in the build option for the unit test
        if @configurator.defines_use_test_definition
          delete_test_definition(test)
        end
        @plugin_manager.post_test( test )
        # restore the project test defines
        if @configurator.project_config_hash.has_key?(def_test_key.to_sym)
          # @configurator.project_config_hash[:defines_test] =
          COLLECTION_DEFINES_TEST_AND_VENDOR.replace(defs_bkp)
          # printf " ---- Restored defines at #{defs_bkp}"
          @configurator.project_config_hash[:project_test_build_output_path] = orig_path
          printf " ************** Restored defines and build path\n"
        end
      end

      # store away what's been processed
      @mocks.concat( mock_list )
      @sources.concat( sources )
    end

    # post-process collected mock list
    @mocks.uniq!

    # post-process collected sources list
    @sources.uniq!
  end


  def refresh_deep_dependencies
    @file_wrapper.rm_f(
      @file_wrapper.directory_listing(
        File.join( @configurator.project_test_dependencies_path, '*' + @configurator.extension_dependencies ) ) )

    @test_invoker_helper.process_deep_dependencies(
      @configurator.collection_all_tests + @configurator.collection_all_source )
  end

end
