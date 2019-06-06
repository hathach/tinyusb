
class Loginator

  constructor :configurator, :project_file_loader, :project_config_manager, :file_wrapper, :system_wrapper


  def setup_log_filepath
    config_files = []
    config_files << @project_file_loader.main_file
    config_files << @project_file_loader.user_file
    config_files.concat( @project_config_manager.options_files )
    config_files.compact!
    config_files.map! { |file| file.ext('') }
    
    log_name = config_files.join( '_' )

    @project_log_filepath = File.join( @configurator.project_log_path, log_name.ext('.log') )
  end


  def log(string, heading=nil)
    return if (not @configurator.project_logging)
  
    output  = "\n[#{@system_wrapper.time_now}]"
    output += " :: #{heading}" if (not heading.nil?)
    output += "\n#{string.strip}\n"

    @file_wrapper.write(@project_log_filepath, output, 'a')
  end
  
end
