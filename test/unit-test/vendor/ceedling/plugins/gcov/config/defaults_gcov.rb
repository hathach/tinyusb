
DEFAULT_GCOV_COMPILER_TOOL = {
    :executable => ENV['CC'].nil? ? FilePathUtils.os_executable_ext('gcc').freeze : ENV['CC'].split[0],
    :name => 'default_gcov_compiler'.freeze,
    :stderr_redirect => StdErrRedirect::NONE.freeze,
    :background_exec => BackgroundExec::NONE.freeze,
    :optional => false.freeze,
    :arguments => [
      "-g".freeze,
      "-fprofile-arcs".freeze,
      "-ftest-coverage".freeze,
      ENV['CC'].nil? ? "" : ENV['CC'].split[1..-1],
      ENV['CPPFLAGS'].nil? ? "" : ENV['CPPFLAGS'].split,
      {"-I\"$\"" => 'COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE_VENDOR'}.freeze,
      {"-I\"$\"" => 'COLLECTION_PATHS_TEST_TOOLCHAIN_INCLUDE'}.freeze,
      {"-D$" => 'COLLECTION_DEFINES_TEST_AND_VENDOR'}.freeze,
      "-DGCOV_COMPILER".freeze,
      "-DCODE_COVERAGE".freeze,
      ENV['CFLAGS'].nil? ? "" : ENV['CFLAGS'].split,
      "-c \"${1}\"".freeze,
      "-o \"${2}\"".freeze
      ].freeze
    }


DEFAULT_GCOV_LINKER_TOOL = {
    :executable => ENV['CCLD'].nil? ? FilePathUtils.os_executable_ext('gcc').freeze : ENV['CCLD'].split[0],
    :name => 'default_gcov_linker'.freeze,
    :stderr_redirect => StdErrRedirect::NONE.freeze,
    :background_exec => BackgroundExec::NONE.freeze,
    :optional => false.freeze,
    :arguments => [
        "-g".freeze,
        "-fprofile-arcs".freeze,
        "-ftest-coverage".freeze,
        ENV['CCLD'].nil? ? "" : ENV['CCLD'].split[1..-1],
        ENV['CFLAGS'].nil? ? "" : ENV['CFLAGS'].split,
        ENV['LDFLAGS'].nil? ? "" : ENV['LDFLAGS'].split,
        "\"${1}\"".freeze,
        "-o \"${2}\"".freeze,
        "${4}".freeze,
        "${5}".freeze,
        ENV['LDLIBS'].nil? ? "" : ENV['LDLIBS'].split
        ].freeze
    }

DEFAULT_GCOV_FIXTURE_TOOL = {
    :executable => '${1}'.freeze,
    :name => 'default_gcov_fixture'.freeze,
    :stderr_redirect => StdErrRedirect::NONE.freeze,
    :background_exec => BackgroundExec::NONE.freeze,
    :optional => false.freeze,
    :arguments => [].freeze
    }

DEFAULT_GCOV_REPORT_TOOL = {
    :executable => ENV['GCOV'].nil? ? FilePathUtils.os_executable_ext('gcov').freeze : ENV['GCOV'].split[0],
    :name => 'default_gcov_report'.freeze,
    :stderr_redirect => StdErrRedirect::NONE.freeze,
    :background_exec => BackgroundExec::NONE.freeze,
    :optional => false.freeze,
    :arguments => [
        "-n".freeze,
        "-p".freeze,
        "-b".freeze,
        {"-o \"$\"" => 'GCOV_BUILD_OUTPUT_PATH'}.freeze,
        "\"${1}\"".freeze
        ].freeze
    }

DEFAULT_GCOV_GCOV_POST_REPORT_TOOL = {
    :executable => ENV['GCOV'].nil? ? FilePathUtils.os_executable_ext('gcov').freeze : ENV['GCOV'].split[0],
    :name => 'default_gcov_gcov_post_report'.freeze,
    :stderr_redirect => StdErrRedirect::NONE.freeze,
    :background_exec => BackgroundExec::NONE.freeze,
    :optional => true.freeze,
    :arguments => [
        "-b".freeze,
        "-c".freeze,
        "-r".freeze,
        "-x".freeze,
        "${1}".freeze
        ].freeze
    }

DEFAULT_GCOV_GCOVR_POST_REPORT_TOOL = {
    :executable => 'gcovr'.freeze,
    :name => 'default_gcov_gcovr_post_report'.freeze,
    :stderr_redirect => StdErrRedirect::NONE.freeze,
    :background_exec => BackgroundExec::NONE.freeze,
    :optional => true.freeze,
    :arguments => [
        "${1}".freeze
        ].freeze
    }

DEFAULT_GCOV_REPORTGENERATOR_POST_REPORT = {
    :executable => 'reportgenerator'.freeze,
    :name => 'default_gcov_reportgenerator_post_report'.freeze,
    :stderr_redirect => StdErrRedirect::NONE.freeze,
    :background_exec => BackgroundExec::NONE.freeze,
    :optional => true.freeze,
    :arguments => [
        "${1}".freeze
        ].freeze
    }

def get_default_config
    return :tools => {
        :gcov_compiler => DEFAULT_GCOV_COMPILER_TOOL,
        :gcov_linker   => DEFAULT_GCOV_LINKER_TOOL,
        :gcov_fixture  => DEFAULT_GCOV_FIXTURE_TOOL,
        :gcov_report   => DEFAULT_GCOV_REPORT_TOOL,
        :gcov_gcov_post_report => DEFAULT_GCOV_GCOV_POST_REPORT_TOOL,
        :gcov_gcovr_post_report => DEFAULT_GCOV_GCOVR_POST_REPORT_TOOL,
        :gcov_reportgenerator_post_report => DEFAULT_GCOV_REPORTGENERATOR_POST_REPORT
    }
end
