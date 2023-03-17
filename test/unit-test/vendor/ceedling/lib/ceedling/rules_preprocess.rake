

# invocations against this rule should only happen when enhanced dependencies are enabled;
# otherwise, dependency tracking will be too shallow and preprocessed files could intermittently
#  fail to be updated when they actually need to be.
rule(/#{PROJECT_TEST_PREPROCESS_FILES_PATH}\/.+/ => [
    proc do |task_name|
      @ceedling[:file_finder].find_test_or_source_or_header_file(task_name)
    end
  ]) do |file|
  if (not @ceedling[:configurator].project_use_deep_dependencies)
    raise 'ERROR: Ceedling preprocessing rule invoked though necessary auxiliary dependency support not enabled.'
  end
  @ceedling[:generator].generate_preprocessed_file(TEST_SYM, file.source)
end


# invocations against this rule can always happen as there are no deeper dependencies to consider
rule(/#{PROJECT_TEST_PREPROCESS_INCLUDES_PATH}\/.+/ => [
    proc do |task_name|
      @ceedling[:file_finder].find_test_or_source_or_header_file(task_name)
    end
  ]) do |file|
  @ceedling[:generator].generate_shallow_includes_list(TEST_SYM, file.source)
end
