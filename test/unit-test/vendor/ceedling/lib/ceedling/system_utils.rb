
class Object
  def deep_clone
    Marshal::load(Marshal.dump(self))
  end
end


##
# Class containing system utility funcions.
class SystemUtils

  constructor :system_wrapper

  ##
  # Sets up the class. 
  def setup
    @tcsh_shell = nil
  end

  ##
  # Checks the system shell to see if it a tcsh shell.
  def tcsh_shell?
    # once run a single time, return state determined at that execution
    return @tcsh_shell if not @tcsh_shell.nil?
  
    result = @system_wrapper.shell_backticks('echo $version')

    if ((result[:exit_code] == 0) and (result[:output].strip =~ /^tcsh/))
      @tcsh_shell = true
    else
      @tcsh_shell = false
    end
  
    return @tcsh_shell
  end
end
