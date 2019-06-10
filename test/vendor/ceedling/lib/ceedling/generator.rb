require 'ceedling/constants'

class Generator

  constructor :configurator,
              :generator_helper,
              :preprocessinator,
              :cmock_builder,
              :generator_test_runner,
              :generator_test_results,
              :flaginator,
              :test_includes_extractor,
              :tool_executor,
              :file_finder,
              :file_path_utils,
              :streaminator,
              :plugin_manager,
              :file_wrapper


  def generate_shallow_includes_list(context, file)
    @preprocessinator.preprocess_shallow_includes(file)
  end

  def generate_preprocessed_file(context, file)
    @streaminator.stdout_puts("Preprocessing #{File.basename(file)}...", Verbosity::NORMAL)
    @preprocessinator.preprocess_file(file)
  end

  def generate_dependencies_file(tool, context, source, object, dependencies)
    @streaminator.stdout_puts("Generating dependencies for #{File.basename(source)}...", Verbosity::NORMAL)

    command =
      @tool_executor.build_command_line(
        tool,
        [], # extra per-file command line parameters
        source,
        dependencies,
        object)

    @tool_executor.exec( command[:line], command[:options] )
  end

  def generate_mock(context, header_filepath)
    arg_hash = {:header_file => header_filepath, :context => context}
    @plugin_manager.pre_mock_generate( arg_hash )

    begin
      @cmock_builder.cmock.setup_mocks( arg_hash[:header_file] )
    rescue
      raise
    ensure
      @plugin_manager.post_mock_generate( arg_hash )
    end
  end

  # test_filepath may be either preprocessed test file or original test file
  def generate_test_runner(context, test_filepath, runner_filepath)
    arg_hash = {:context => context, :test_file => test_filepath, :runner_file => runner_filepath}
    @plugin_manager.pre_runner_generate(arg_hash)

    # collect info we need
    module_name = File.basename(arg_hash[:test_file])
    test_cases  = @generator_test_runner.find_test_cases( @file_finder.find_test_from_runner_path(runner_filepath) )
    mock_list   = @test_includes_extractor.lookup_raw_mock_list(arg_hash[:test_file])

    @streaminator.stdout_puts("Generating runner for #{module_name}...", Verbosity::NORMAL)

    test_file_includes = [] # Empty list for now, since apparently unused

    # build runner file
    begin
      @generator_test_runner.generate(module_name, runner_filepath, test_cases, mock_list, test_file_includes)
    rescue
      raise
    ensure
      @plugin_manager.post_runner_generate(arg_hash)
    end
  end

  def generate_object_file(tool, operation, context, source, object, list='', dependencies='')
    shell_result = {}
    arg_hash = {:tool => tool, :operation => operation, :context => context, :source => source, :object => object, :list => list, :dependencies => dependencies}
    @plugin_manager.pre_compile_execute(arg_hash)

    @streaminator.stdout_puts("Compiling #{File.basename(arg_hash[:source])}...", Verbosity::NORMAL)
    command =
      @tool_executor.build_command_line( arg_hash[:tool],
                                         @flaginator.flag_down( operation, context, source ),
                                         arg_hash[:source],
                                         arg_hash[:object],
                                         arg_hash[:list],
                                         arg_hash[:dependencies])

    begin
      shell_result = @tool_executor.exec( command[:line], command[:options] )
    rescue ShellExecutionException => ex
      shell_result = ex.shell_result
      raise ex
    ensure
      arg_hash[:shell_result] = shell_result
      @plugin_manager.post_compile_execute(arg_hash)
    end
  end

  def generate_executable_file(tool, context, objects, executable, map='', libraries=[])
    shell_result = {}
    arg_hash = { :tool => tool,
                 :context => context,
                 :objects => objects,
                 :executable => executable,
                 :map => map,
                 :libraries => libraries
               }

    @plugin_manager.pre_link_execute(arg_hash)

    @streaminator.stdout_puts("Linking #{File.basename(arg_hash[:executable])}...", Verbosity::NORMAL)
    command =
      @tool_executor.build_command_line( arg_hash[:tool],
                                         @flaginator.flag_down( OPERATION_LINK_SYM, context, executable ),
                                         arg_hash[:objects],
                                         arg_hash[:executable],
                                         arg_hash[:map],
                                         arg_hash[:libraries]
                                       )

    begin
      shell_result = @tool_executor.exec( command[:line], command[:options] )
    rescue ShellExecutionException => ex
      notice =    "\n" +
                  "NOTICE: If the linker reports missing symbols, the following may be to blame:\n" +
                  "  1. Test lacks #include statements corresponding to needed source files.\n" +
                  "  2. Project search paths do not contain source files corresponding to #include statements in the test.\n"

      if (@configurator.project_use_mocks)
        notice += "  3. Test does not #include needed mocks.\n\n"
      else
        notice += "\n"
      end

      @streaminator.stderr_puts(notice, Verbosity::COMPLAIN)
      shell_result = ex.shell_result
      raise ''
    ensure
      arg_hash[:shell_result] = shell_result
      @plugin_manager.post_link_execute(arg_hash)
    end
  end

  def generate_test_results(tool, context, executable, result)
    arg_hash = {:tool => tool, :context => context, :executable => executable, :result_file => result}
    @plugin_manager.pre_test_fixture_execute(arg_hash)

    @streaminator.stdout_puts("Running #{File.basename(arg_hash[:executable])}...", Verbosity::NORMAL)

    # Unity's exit code is equivalent to the number of failed tests, so we tell @tool_executor not to fail out if there are failures
    # so that we can run all tests and collect all results
    command = @tool_executor.build_command_line(arg_hash[:tool], [], arg_hash[:executable])
    command[:options][:boom] = false
    shell_result = @tool_executor.exec( command[:line], command[:options] )
    shell_result[:exit_code] = 0 #Don't Let The Failure Count Make Us Believe Things Aren't Working
    @generator_helper.test_results_error_handler(executable, shell_result)

    processed = @generator_test_results.process_and_write_results( shell_result,
                                                                   arg_hash[:result_file],
                                                                   @file_finder.find_test_from_file_path(arg_hash[:executable]) )

    arg_hash[:result_file]  = processed[:result_file]
    arg_hash[:results]      = processed[:results]
    arg_hash[:shell_result] = shell_result # for raw output display if no plugins for formatted display

    @plugin_manager.post_test_fixture_execute(arg_hash)
  end

end
