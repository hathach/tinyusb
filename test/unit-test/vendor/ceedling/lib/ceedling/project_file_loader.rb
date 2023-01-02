require 'ceedling/constants'


class ProjectFileLoader

  attr_reader :main_file, :user_file

  constructor :yaml_wrapper, :stream_wrapper, :system_wrapper, :file_wrapper

  def setup
    @main_file = nil
    @mixin_files = []
    @user_file = nil

    @main_project_filepath = ''
    @mixin_project_filepaths = []
    @user_project_filepath = ''
  end


  def find_project_files
    # first go hunting for optional user project file by looking for environment variable and then default location on disk
    user_filepath = @system_wrapper.env_get('CEEDLING_USER_PROJECT_FILE')

    if ( not user_filepath.nil? and @file_wrapper.exist?(user_filepath) )
      @user_project_filepath = user_filepath
    elsif (@file_wrapper.exist?(DEFAULT_CEEDLING_USER_PROJECT_FILE))
      @user_project_filepath = DEFAULT_CEEDLING_USER_PROJECT_FILE
    end

    # next check for mixin project files by looking for environment variable
    mixin_filepaths = @system_wrapper.env_get('CEEDLING_MIXIN_PROJECT_FILES')
    if ( not mixin_filepaths.nil? )
      mixin_filepaths.split(File::PATH_SEPARATOR).each do |filepath|
        if ( @file_wrapper.exist?(filepath) )
          @mixin_project_filepaths.push(filepath)
        end
      end
    end

    # next check for main project file by looking for environment variable and then default location on disk;
    # blow up if we don't find this guy -- like, he's so totally important
    main_filepath = @system_wrapper.env_get('CEEDLING_MAIN_PROJECT_FILE')

    if ( not main_filepath.nil? and @file_wrapper.exist?(main_filepath) )
      @main_project_filepath = main_filepath
    elsif (@file_wrapper.exist?(DEFAULT_CEEDLING_MAIN_PROJECT_FILE))
      @main_project_filepath = DEFAULT_CEEDLING_MAIN_PROJECT_FILE
    else
      # no verbosity checking since this is lowest level reporting anyhow &
      # verbosity checking depends on configurator which in turns needs this class (circular dependency)
      @stream_wrapper.stderr_puts('Found no Ceedling project file (*.yml)')
      raise
    end

    @main_file = File.basename( @main_project_filepath )
    @mixin_project_filepaths.each do |filepath|
      @mixin_files.push(File.basename( filepath ))
    end
    @user_file = File.basename( @user_project_filepath ) if ( not @user_project_filepath.empty? )
  end

  def yaml_merger(y1, y2)
    o1 = y1
    y2.each_pair do |k,v|
      if o1[k].nil?
        o1[k] = v
      else
        if (o1[k].instance_of? Hash)
          o1[k] = yaml_merger(o1[k], v)
        elsif (o1[k].instance_of? Array)
          o1[k] += v
        else
          o1[k] = v
        end
      end
    end
    return o1
  end

  def load_project_config
    config_hash = @yaml_wrapper.load(@main_project_filepath)

    # if there are mixin project files, then use them
    @mixin_project_filepaths.each do |filepath|
      mixin = @yaml_wrapper.load(filepath)
      config_hash = yaml_merger( config_hash, mixin )
    end

    # if there's a user project file, then use it
    if ( not @user_project_filepath.empty? )
      user_hash = @yaml_wrapper.load(@user_project_filepath)
      config_hash = yaml_merger( config_hash, user_hash )
    end

    return config_hash
  end

end
