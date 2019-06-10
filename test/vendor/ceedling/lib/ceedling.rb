##
# This module defines the interface for interacting with and loading a project
# with Ceedling.
module Ceedling
  ##
  # Returns the location where the gem is installed.
  # === Return
  # _String_ - The location where the gem lives.
  def self.location
    File.join( File.dirname(__FILE__), '..')
  end

  ##
  # Return the path to the "built-in" plugins.
  # === Return
  # _String_ - The path where the default plugins live.
  def self.load_path
    File.join( self.location, 'plugins')
  end

  ##
  # Return the path to the Ceedling Rakefile
  # === Return
  # _String_
  def self.rakefile
    File.join( self.location, 'lib', 'ceedling', 'rakefile.rb' )
  end

  ##
  # This method selects the project file that Ceedling will use by setting the
  # CEEDLING_MAIN_PROJECT_FILE environment variable before loading the ceedling
  # rakefile. A path supplied as an argument to this method will override the
  # current value of the environment variable. If no path is supplied as an
  # argument then the existing value of the environment variable is used. If
  # the environment variable has not been set and no argument has been supplied
  # then a default path of './project.yml' will be used.
  #
  # === Arguments
  # +options+ _Hash_::
  #   A hash containing the options for ceedling. Currently the following
  #   options are supported:
  #   * +config+ - The path to the project YAML configuration file.
  #   * +root+ - The root of the project directory.
  #   * +prefix+ - A prefix to prepend to plugin names in order to determine the
  #     corresponding gem name.
  #   * +plugins+ - The list of ceedling plugins to load
  def self.load_project(options = {})
    # Make sure our path to the yaml file is setup
    if options.has_key? :config
      ENV['CEEDLING_MAIN_PROJECT_FILE'] = options[:config]
    elsif ENV['CEEDLING_MAIN_PROJECT_FILE'].nil?
      ENV['CEEDLING_MAIN_PROJECT_FILE'] = './project.yml'
    end

    # Register the plugins
    if options.has_key? :plugins
      options[:plugins].each do |plugin|
        register_plugin( plugin, options[:prefix] )
      end
    end

    # Define the root of the project if specified
    Object.const_set('PROJECT_ROOT', options[:root]) if options.has_key? :root

    # Load ceedling
    load "#{self.rakefile}"
  end

  ##
  # Register a plugin for ceedling to use when a project is loaded. This method
  # *must* be called prior to calling the _load_project_ method.
  #
  # This method is intended to be used for loading plugins distributed via the
  # RubyGems mechanism. As such, the following gem structure is assumed for
  # plugins.
  #
  # * The gem name must be prefixed with 'ceedling-' followed by the plugin
  #   name (ex. 'ceedling-bullseye')
  #
  # * The contents of the plugin must be isntalled into a subdirectory of
  #   the gem with the same name as the plugin (ex. 'bullseye/')
  #
  # === Arguments
  # +name+ _String_:: The name of the plugin to load.
  # +prefix+ _String_::
  #   (optional, default = nil) The prefix to use for the full gem name.
  def self.register_plugin(name, prefix=nil)
    # Figure out the full name of the gem and location
    prefix   ||= 'ceedling-'
    gem_name   = prefix + name
    gem_dir    = Gem::Specification.find_by_name(gem_name).gem_dir()

    # Register the plugin with Ceedling
    require 'ceedling/defaults'
    DEFAULT_CEEDLING_CONFIG[:plugins][:enabled]    << name
    DEFAULT_CEEDLING_CONFIG[:plugins][:load_paths] << gem_dir
  end
end

