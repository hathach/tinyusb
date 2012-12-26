require 'constants'


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
  
  def setup_and_invoke(tests, context=TEST_SYM, options={:force_run => true})
  
    @tests = tests

    @project_config_manager.process_test_config_change
  
    @tests.each do |test|
      # announce beginning of test run
      header = "Test '#{File.basename(test)}'"
      @streaminator.stdout_puts("\n\n#{header}\n#{'-' * header.length}")

      begin
        @plugin_manager.pre_test
        
        # collect up test fixture pieces & parts
        runner       = @file_path_utils.form_runner_filepath_from_test( test )
        mock_list    = @preprocessinator.preprocess_test_and_invoke_test_mocks( test )
        sources      = @test_invoker_helper.extract_sources( test )
        extras       = @configurator.collection_test_fixture_extra_link_objects
        core         = [test] + mock_list + sources
        objects      = @file_path_utils.form_test_build_objects_filelist( [runner] + core + extras )
        results_pass = @file_path_utils.form_pass_results_filepath( test )
        results_fail = @file_path_utils.form_fail_results_filepath( test )
        
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

        # 3, 2, 1... launch
        @task_invoker.invoke_test_results( results_pass )        
      rescue => e
        @build_invoker_utils.process_exception( e, context )
      ensure
        @plugin_manager.post_test        
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
