require 'plugin'
require 'constants'

class WarningsReport < Plugin

  def setup
    @stderr_redirect = nil
    @log_paths = {}
  end

  def pre_compile_execute( arg_hash )
    # at beginning of compile, override tool's stderr_redirect so we can parse $stderr + $stdout
    set_stderr_redirect( arg_hash )
  end
  
  def post_compile_execute( arg_hash )
    # after compilation, grab output for parsing/logging, restore stderr_redirect, log warning if it exists
    output = arg_hash[:shell_result][:output]
    restore_stderr_redirect( arg_hash )
    write_warning_log( arg_hash[:context], output )
  end

  def pre_link_execute( arg_hash )
    # at beginning of link, override tool's stderr_redirect so we can parse $stderr + $stdout
    set_stderr_redirect( arg_hash )
  end
  
  def post_link_execute( arg_hash )
    # after linking, grab output for parsing/logging, restore stderr_redirect, log warning if it exists
    output = arg_hash[:shell_result][:output]
    restore_stderr_redirect( arg_hash )
    write_warning_log( arg_hash[:context], output )
  end

  private
  
  def set_stderr_redirect( hash )
    @stderr_redirect = hash[:tool][:stderr_redirect]
    hash[:tool][:stderr_redirect] = StdErrRedirect::AUTO    
  end
  
  def restore_stderr_redirect( hash )
    hash[:tool][:stderr_redirect] = @stderr_redirect    
  end
  
  def write_warning_log( context, output )
    # if $stderr/$stdout contain "warning", log it
    if (output =~ /warning/i)
      # generate a log path & file io write flags
      logging = generate_log_path( context )
      @ceedling[:file_wrapper].write( logging[:path], output + "\n", logging[:flags] ) if (not logging.nil?)
    end
  end

  def generate_log_path( context )
    # if path has already been generated, return it & 'append' file io flags (append to log)
    return { :path => @log_paths[context], :flags => 'a' } if (not @log_paths[context].nil?)
    
    # first time through, generate path & 'write' file io flags (create new log)
    base_path = File.join( PROJECT_BUILD_ARTIFACTS_ROOT, context.to_s )
    file_path = File.join( base_path, 'warnings.log' )
    
    if (@ceedling[:file_wrapper].exist?( base_path ))
      @log_paths[context] = file_path
      return { :path => file_path, :flags => 'w' }
    end
    
    return nil
  end

end