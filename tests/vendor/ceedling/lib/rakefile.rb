require 'fileutils'

# get directory containing this here file, back up one directory, and expand to full path
CEEDLING_ROOT    = File.expand_path(File.dirname(__FILE__) + '/..')
CEEDLING_LIB     = File.join(CEEDLING_ROOT, 'lib')
CEEDLING_VENDOR  = File.join(CEEDLING_ROOT, 'vendor')
CEEDLING_RELEASE = File.join(CEEDLING_ROOT, 'release')

$LOAD_PATH.unshift( CEEDLING_LIB )
$LOAD_PATH.unshift( File.join(CEEDLING_VENDOR, 'unity/auto') )
$LOAD_PATH.unshift( File.join(CEEDLING_VENDOR, 'diy/lib') )
$LOAD_PATH.unshift( File.join(CEEDLING_VENDOR, 'constructor/lib') )
$LOAD_PATH.unshift( File.join(CEEDLING_VENDOR, 'cmock/lib') )
$LOAD_PATH.unshift( File.join(CEEDLING_VENDOR, 'deep_merge/lib') )

require 'rake'

require 'diy'
require 'constructor'

require 'constants'
require 'target_loader'


# construct all our objects
@ceedling = DIY::Context.from_yaml( File.read( File.join(CEEDLING_LIB, 'objects.yml') ) )
@ceedling.build_everything

# one-stop shopping for all our setup and such after construction
@ceedling[:setupinator].ceedling = @ceedling

project_config =
  begin
    cfg = @ceedling[:setupinator].load_project_files
    TargetLoader.inspect(cfg, ENV['TARGET'])
  rescue TargetLoader::NoTargets
    cfg
  rescue TargetLoader::RequestReload
    @ceedling[:setupinator].load_project_files
  end

@ceedling[:setupinator].do_setup( project_config )


# tell all our plugins we're about to do something
@ceedling[:plugin_manager].pre_build

# load rakefile component files (*.rake)
PROJECT_RAKEFILE_COMPONENT_FILES.each { |component| load(component) }

# tell rake to shut up by default (overridden in verbosity / debug tasks as appropriate)
verbose(false)


# end block always executed following rake run
END {
  # cache our input configurations to use in comparison upon next execution
  @ceedling[:cacheinator].cache_test_config( @ceedling[:setupinator].config_hash )    if (@ceedling[:task_invoker].test_invoked?)
  @ceedling[:cacheinator].cache_release_config( @ceedling[:setupinator].config_hash ) if (@ceedling[:task_invoker].release_invoked?)

  # delete all temp files unless we're in debug mode
  if (not @ceedling[:configurator].project_debug)
    @ceedling[:file_wrapper].rm_f( @ceedling[:file_wrapper].directory_listing( File.join(@ceedling[:configurator].project_temp_path, '*') ))
  end

	# only perform these final steps if we got here without runtime exceptions or errors
	if (@ceedling[:system_wrapper].ruby_success)

    # tell all our plugins the build is done and process results
	  @ceedling[:plugin_manager].post_build
	  @ceedling[:plugin_manager].print_plugin_failures
	  exit(1) if (@ceedling[:plugin_manager].plugins_failed?)
	end
}
