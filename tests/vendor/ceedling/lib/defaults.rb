require 'constants'
require 'system_wrapper'
require 'file_path_utils'


DEFAULT_TEST_COMPILER_TOOL = {
  :executable => FilePathUtils.os_executable_ext('gcc').freeze,
  :name => 'default_test_compiler'.freeze,
  :stderr_redirect => StdErrRedirect::NONE.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [
    {"-I\"$\"" => 'COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE_VENDOR'}.freeze,
    {"-I\"$\"" => 'COLLECTION_PATHS_TEST_TOOLCHAIN_INCLUDE'}.freeze,
    {"-D$" => 'COLLECTION_DEFINES_TEST_AND_VENDOR'}.freeze,
    "-DGNU_COMPILER".freeze,
    "-c \"${1}\"".freeze,
    "-o \"${2}\"".freeze,
    # gcc's list file output options are complex; no use of ${3} parameter in default config    
    ].freeze
  }

DEFAULT_TEST_LINKER_TOOL = {
  :executable => FilePathUtils.os_executable_ext('gcc').freeze,
  :name => 'default_test_linker'.freeze,
  :stderr_redirect => StdErrRedirect::NONE.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [
    "\"${1}\"".freeze,
    "-o \"${2}\"".freeze,
    ].freeze
  }
  
DEFAULT_TEST_FIXTURE_TOOL = {
  :executable => '${1}'.freeze,
  :name => 'default_test_fixture'.freeze,
  :stderr_redirect => StdErrRedirect::AUTO.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [].freeze
  }



DEFAULT_TEST_INCLUDES_PREPROCESSOR_TOOL = {
  :executable => FilePathUtils.os_executable_ext('cpp').freeze,
  :name => 'default_test_includes_preprocessor'.freeze,
  :stderr_redirect => StdErrRedirect::NONE.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [
    '-MM'.freeze,
    '-MG'.freeze,
    # avoid some possibility of deep system lib header file complications by omitting vendor paths
    # if cpp is run on *nix system, escape spaces in paths; if cpp on windows just use the paths collection as is
    {"-I\"$\"" => "{SystemWrapper.windows? ? COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE : COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE.map{|path| path.gsub(\/ \/, \'\\\\ \') }}"}.freeze,
    {"-D$" => 'COLLECTION_DEFINES_TEST_AND_VENDOR'}.freeze,
    {"-D$" => 'DEFINES_TEST_PREPROCESS'}.freeze,
    "-DGNU_PREPROCESSOR".freeze,
    '-w'.freeze,
    '-nostdinc'.freeze,
    "\"${1}\"".freeze
    ].freeze
  }

DEFAULT_TEST_FILE_PREPROCESSOR_TOOL = {
  :executable => FilePathUtils.os_executable_ext('gcc').freeze,
  :name => 'default_test_file_preprocessor'.freeze,
  :stderr_redirect => StdErrRedirect::NONE.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [
    '-E'.freeze,
    {"-I\"$\"" => 'COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE_VENDOR'}.freeze,
    {"-I\"$\"" => 'COLLECTION_PATHS_TEST_TOOLCHAIN_INCLUDE'}.freeze,
    {"-D$" => 'COLLECTION_DEFINES_TEST_AND_VENDOR'}.freeze,
    {"-D$" => 'DEFINES_TEST_PREPROCESS'}.freeze,
    "-DGNU_PREPROCESSOR".freeze,
    "\"${1}\"".freeze,
    "-o \"${2}\"".freeze
    ].freeze
  }

DEFAULT_TEST_DEPENDENCIES_GENERATOR_TOOL = {
  :executable => FilePathUtils.os_executable_ext('gcc').freeze,
  :name => 'default_test_dependencies_generator'.freeze,
  :stderr_redirect => StdErrRedirect::NONE.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [
    {"-I\"$\"" => 'COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE_VENDOR'}.freeze,
    {"-I\"$\"" => 'COLLECTION_PATHS_TEST_TOOLCHAIN_INCLUDE'}.freeze,
    {"-D$" => 'COLLECTION_DEFINES_TEST_AND_VENDOR'}.freeze, 
    {"-D$" => 'DEFINES_TEST_PREPROCESS'}.freeze,     
    "-DGNU_PREPROCESSOR".freeze,
    "-MT \"${3}\"".freeze,
    '-MM'.freeze,
    '-MD'.freeze,
    '-MG'.freeze,
    "-MF \"${2}\"".freeze,
    "-c \"${1}\"".freeze,
    ].freeze
  }

DEFAULT_RELEASE_DEPENDENCIES_GENERATOR_TOOL = {
  :executable => FilePathUtils.os_executable_ext('gcc').freeze,
  :name => 'default_release_dependencies_generator'.freeze,
  :stderr_redirect => StdErrRedirect::NONE.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [
    {"-I\"$\"" => 'COLLECTION_PATHS_SOURCE_INCLUDE_VENDOR'}.freeze,
    {"-I\"$\"" => 'COLLECTION_PATHS_RELEASE_TOOLCHAIN_INCLUDE'}.freeze,
    {"-D$" => 'COLLECTION_DEFINES_RELEASE_AND_VENDOR'}.freeze,
    {"-D$" => 'DEFINES_RELEASE_PREPROCESS'}.freeze,
    "-DGNU_PREPROCESSOR".freeze,
    "-MT \"${3}\"".freeze,
    '-MM'.freeze,
    '-MD'.freeze,
    '-MG'.freeze,
    "-MF \"${2}\"".freeze,
    "-c \"${1}\"".freeze,
    ].freeze
  }


DEFAULT_RELEASE_COMPILER_TOOL = {
  :executable => FilePathUtils.os_executable_ext('gcc').freeze,
  :name => 'default_release_compiler'.freeze,
  :stderr_redirect => StdErrRedirect::NONE.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [
    {"-I\"$\"" => 'COLLECTION_PATHS_SOURCE_INCLUDE_VENDOR'}.freeze,
    {"-I\"$\"" => 'COLLECTION_PATHS_RELEASE_TOOLCHAIN_INCLUDE'}.freeze,
    {"-D$" => 'COLLECTION_DEFINES_RELEASE_AND_VENDOR'}.freeze,
    "-DGNU_COMPILER".freeze,
    "-c \"${1}\"".freeze,
    "-o \"${2}\"".freeze,
    # gcc's list file output options are complex; no use of ${3} parameter in default config    
    ].freeze
  }

DEFAULT_RELEASE_ASSEMBLER_TOOL = {
  :executable => FilePathUtils.os_executable_ext('as').freeze,
  :name => 'default_release_assembler'.freeze,
  :stderr_redirect => StdErrRedirect::NONE.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [
    {"-I\"$\"" => 'COLLECTION_PATHS_SOURCE_AND_INCLUDE'}.freeze,
    "\"${1}\"".freeze,
    "-o \"${2}\"".freeze,
    ].freeze
  }

DEFAULT_RELEASE_LINKER_TOOL = {
  :executable => FilePathUtils.os_executable_ext('gcc').freeze,
  :name => 'default_release_linker'.freeze,
  :stderr_redirect => StdErrRedirect::NONE.freeze,
  :background_exec => BackgroundExec::NONE.freeze,
  :optional => false.freeze,
  :arguments => [
    "\"${1}\"".freeze,
    "-o \"${2}\"".freeze,
    ].freeze
  }

  
DEFAULT_TOOLS_TEST = {
  :tools => {
    :test_compiler => DEFAULT_TEST_COMPILER_TOOL,
    :test_linker   => DEFAULT_TEST_LINKER_TOOL,
    :test_fixture  => DEFAULT_TEST_FIXTURE_TOOL,
    }
  }
  
DEFAULT_TOOLS_TEST_PREPROCESSORS = {
  :tools => {
    :test_includes_preprocessor => DEFAULT_TEST_INCLUDES_PREPROCESSOR_TOOL,
    :test_file_preprocessor     => DEFAULT_TEST_FILE_PREPROCESSOR_TOOL,
    }
  }

DEFAULT_TOOLS_TEST_DEPENDENCIES = {
  :tools => {
    :test_dependencies_generator => DEFAULT_TEST_DEPENDENCIES_GENERATOR_TOOL,
    }
  }


DEFAULT_TOOLS_RELEASE = {
  :tools => {
    :release_compiler => DEFAULT_RELEASE_COMPILER_TOOL,
    :release_linker   => DEFAULT_RELEASE_LINKER_TOOL,
    }
  }

DEFAULT_TOOLS_RELEASE_ASSEMBLER = {
  :tools => {
    :release_assembler => DEFAULT_RELEASE_ASSEMBLER_TOOL,
    }
  }

DEFAULT_TOOLS_RELEASE_DEPENDENCIES = {
  :tools => {
    :release_dependencies_generator => DEFAULT_RELEASE_DEPENDENCIES_GENERATOR_TOOL,
    }
  }

  
DEFAULT_RELEASE_TARGET_NAME = 'project'

DEFAULT_CEEDLING_CONFIG = {
    :project => {
      # :build_root must be set by user
      :use_exceptions => true,
      :use_mocks => true,
      :compile_threads => 1,
      :test_threads => 1,
      :use_test_preprocessor => false,
      :use_deep_dependencies => false,
      :test_file_prefix => 'test_',
      :options_paths => [],
      :release_build => false,
    },

    :release_build => {
      # :output is set while building configuration -- allows smart default system-dependent file extension handling
      :use_assembly => false,
      :artifacts => [],
    },

    :paths => {
      :test => [],   # must be populated by user
      :source => [], # must be populated by user
      :support => [],
      :include => [],
      :test_toolchain_include => [],
      :release_toolchain_include => [],
    },
    
    :files => {
      :test => [],
      :source => [],
      :assembly => [],
      :support => [],
      :include => [],
    },
    
    # unlike other top-level entries, environment's value is an array to preserve order
    :environment => [
      # when evaluated, this provides wider text field for rake task comments
      {:rake_columns => '120'},
    ],
    
    :defines => {
      :test => [],
      :test_preprocess => [],
      :release => [],
      :release_preprocess => [],
    },
    
    :flags => {},
    
    :extension => {
      :header => '.h',
      :source => '.c',
      :assembly => '.s',
      :object => '.o',
      :executable => ( SystemWrapper.windows? ? EXTENSION_WIN_EXE : EXTENSION_NONWIN_EXE ),
      :map => '.map',
      :list => '.lst',
      :testpass => '.pass',
      :testfail => '.fail',
      :dependencies => '.d',
    },

    :unity => {
      :defines => []
    },

    :cmock => {
      :defines => []
    },

    :cexception => {
      :defines => []
    },

    :test_runner => {
      :includes => [],
      :file_suffix => '_runner',
    },

    # all tools populated while building up config structure
    :tools => {},

    # empty argument lists for default tools
    # (these can be overridden in project file to add arguments to tools without totally redefining tools)
    :test_compiler => { :arguments => [] },
    :test_linker   => { :arguments => [] },
    :test_fixture  => { 
      :arguments => [],
      :link_objects => [], # compiled object files to always be linked in (e.g. cmock.o if using mocks)
    },
    :test_includes_preprocessor  => { :arguments => [] },
    :test_file_preprocessor      => { :arguments => [] },
    :test_dependencies_generator => { :arguments => [] },
    :release_compiler  => { :arguments => [] },
    :release_linker    => { :arguments => [] },
    :release_assembler => { :arguments => [] },
    :release_dependencies_generator => { :arguments => [] },

    :plugins => {
      :load_paths => [],
      :enabled => [],
    }
  }.freeze

  
DEFAULT_TESTS_RESULTS_REPORT_TEMPLATE = %q{
% ignored        = hash[:results][:counts][:ignored]
% failed         = hash[:results][:counts][:failed]
% stdout_count   = hash[:results][:counts][:stdout]
% header_prepend = ((hash[:header].length > 0) ? "#{hash[:header]}: " : '')
% banner_width   = 25 + header_prepend.length # widest message

% if (ignored > 0)
<%=@ceedling[:plugin_reportinator].generate_banner(header_prepend + 'IGNORED UNIT TEST SUMMARY')%>
%   hash[:results][:ignores].each do |ignore|
%     ignore[:collection].each do |item|
<%=ignore[:source][:path]%><%=File::SEPARATOR%><%=ignore[:source][:file]%>:<%=item[:line]%>:<%=item[:test]%>
% if (item[:message].length > 0)
: "<%=item[:message]%>"
% else
<%="\n"%>
% end
%     end
%   end

% end
% if (failed > 0)
<%=@ceedling[:plugin_reportinator].generate_banner(header_prepend + 'FAILED UNIT TEST SUMMARY')%>
%   hash[:results][:failures].each do |failure|
%     failure[:collection].each do |item|
<%=failure[:source][:path]%><%=File::SEPARATOR%><%=failure[:source][:file]%>:<%=item[:line]%>:<%=item[:test]%>
% if (item[:message].length > 0)
: "<%=item[:message]%>"
% else
<%="\n"%>
% end
%     end
%   end

% end
% if (stdout_count > 0)
<%=@ceedling[:plugin_reportinator].generate_banner(header_prepend + 'UNIT TEST OTHER OUTPUT')%>
%   hash[:results][:stdout].each do |string|
%     string[:collection].each do |item|
<%=string[:source][:path]%><%=File::SEPARATOR%><%=string[:source][:file]%>: "<%=item%>"
%     end
%   end

% end
% total_string = hash[:results][:counts][:total].to_s
% format_string = "%#{total_string.length}i"
<%=@ceedling[:plugin_reportinator].generate_banner(header_prepend + 'OVERALL UNIT TEST SUMMARY')%>
% if (hash[:results][:counts][:total] > 0)
TESTED:  <%=hash[:results][:counts][:total].to_s%>
PASSED:  <%=sprintf(format_string, hash[:results][:counts][:passed])%>
FAILED:  <%=sprintf(format_string, failed)%>
IGNORED: <%=sprintf(format_string, ignored)%>
% else

No tests executed.
% end

}
