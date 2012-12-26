require 'constants'
require 'file_path_utils'


desc "Display build environment version info."
task :version do
  tools = [
      ['  Ceedling', CEEDLING_ROOT],
      ['CException', File.join( CEEDLING_VENDOR, CEXCEPTION_ROOT_PATH)],
      ['     CMock', File.join( CEEDLING_VENDOR, CMOCK_ROOT_PATH)],
      ['     Unity', File.join( CEEDLING_VENDOR, UNITY_ROOT_PATH)],
    ]
  
  tools.each do |tool|
    name      = tool[0]
    base_path = tool[1]
    
    version_string = @ceedling[:file_wrapper].read( File.join(base_path, 'release', 'version.info') ).strip
    build_string   = @ceedling[:file_wrapper].read( File.join(base_path, 'release', 'build.info') ).strip
    puts "#{name}:: #{version_string.empty? ? '#.#.' : (version_string + '.')}#{build_string.empty? ? '?' : build_string}"
  end
end


desc "Set verbose output (silent:[#{Verbosity::SILENT}] - obnoxious:[#{Verbosity::OBNOXIOUS}])."
task :verbosity, :level do |t, args|
  verbosity_level = args.level.to_i
  
  if (PROJECT_USE_MOCKS)
    # don't store verbosity level in setupinator's config hash, use a copy;
    # otherwise, the input configuration will change and trigger entire project rebuilds
    hash = @ceedling[:setupinator].config_hash[:cmock].clone
    hash[:verbosity] = verbosity_level

    @ceedling[:cmock_builder].manufacture( hash )  
  end

  @ceedling[:configurator].project_verbosity = verbosity_level

  # control rake's verbosity with new setting
  verbose( ((verbosity_level >= Verbosity::OBNOXIOUS) ? true : false) )
end


desc "Enable logging"
task :logging do
  @ceedling[:configurator].project_logging = true
end


# non advertised debug task
task :debug do
  Rake::Task[:verbosity].invoke(Verbosity::DEBUG)
  Rake.application.options.trace = true
  @ceedling[:configurator].project_debug = true
end


# non advertised sanity checking task
task :sanity_checks, :level do |t, args|
  check_level = args.level.to_i
  @ceedling[:configurator].sanity_checks = check_level
end


# list expanded environment variables
if (not ENVIRONMENT.empty?)
desc "List all configured environment variables."
task :environment do
  ENVIRONMENT.each do |env|
    env.each_key do |key|
      name = key.to_s.upcase
      puts " - #{name}: \"#{env[key]}\""
    end
  end  
end
end


namespace :options do

  COLLECTION_PROJECT_OPTIONS.each do |option_path|
    option = File.basename(option_path, '.yml')

    desc "Merge #{option} project options."
    task option.downcase.to_sym do
      # @ceedling[:setupinator].reset_defaults( @ceedling[:setupinator].config_hash )
      hash = @ceedling[:project_config_manager].merge_options( @ceedling[:setupinator].config_hash, option_path )
      @ceedling[:setupinator].do_setup( hash )
    end
  end

end


# do not present task if there's no plugins
if (not PLUGINS_ENABLED.empty?)
desc "Execute plugin result summaries (no build triggering)."
task :summary do
	@ceedling[:plugin_manager].summary
  puts "\nNOTE: Summaries may be out of date with project sources.\n\n"
end
end

