require 'ceedling/constants'
require 'ceedling/file_path_utils'
require 'ceedling/version'

desc "Display build environment version info."
task :version do
  puts "   Ceedling:: #{Ceedling::Version::CEEDLING}"
  puts "      Unity:: #{Ceedling::Version::UNITY}"
  puts "      CMock:: #{Ceedling::Version::CMOCK}"
  puts " CException:: #{Ceedling::Version::CEXCEPTION}"
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

# non advertised catch for calling upgrade in the wrong place
task :upgrade do
  puts "WARNING: You're currently IN your project directory. Take a step out and try"
  puts "again if you'd like to perform an upgrade."
end

# list expanded environment variables
if (not ENVIRONMENT.empty?)
desc "List all configured environment variables."
task :environment do
  env_list = []
  ENVIRONMENT.each do |env|
    env.each_key do |key|
      name = key.to_s.upcase
	  env_list.push(" - #{name}: \"#{env[key]}\"")
    end
  end
  env_list.sort.each do |env_line|
	puts env_line
  end
end
end

namespace :options do

  COLLECTION_PROJECT_OPTIONS.each do |option_path|
    option = File.basename(option_path, '.yml')

    desc "Merge #{option} project options."
    task option.to_sym do
      hash = @ceedling[:project_config_manager].merge_options( @ceedling[:setupinator].config_hash, option_path )
      @ceedling[:setupinator].do_setup( hash )
      if @ceedling[:configurator].project_release_build
        load(File.join(CEEDLING_LIB, 'ceedling', 'rules_release.rake'))
      end
    end
  end

  # This is to give nice errors when typing options
  rule /^options:.*/ do |t, args|
    filename = t.to_s.split(':')[-1] + '.yml'
    filelist = COLLECTION_PROJECT_OPTIONS.map{|s| File.basename(s) }
    @ceedling[:file_finder].find_file_from_list(filename, filelist, :error)
  end

  # This will output the fully-merged tools options to their own project.yml file
  desc "Export tools options to a new project file"
  task :export, :filename do |t, args|
    outfile = args.filename || 'tools.yml'
    toolcfg = {}
    @ceedling[:configurator].project_config_hash.each_pair do |k,v|
      toolcfg[k] = v if (k.to_s[0..5] == 'tools_')
    end
    File.open(outfile,'w') {|f| f << toolcfg.to_yaml({:indentation => 2})}
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

