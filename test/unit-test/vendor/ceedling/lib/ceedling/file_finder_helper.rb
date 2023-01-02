require 'fileutils'
require 'ceedling/constants' # for Verbosity enumeration

class FileFinderHelper

  constructor :streaminator
  
  
  def find_file_in_collection(file_name, file_list, complain, extra_message="")
    file_to_find = nil
    
    file_list.each do |item|
      base_file = File.basename(item)

      # case insensitive comparison
      if (base_file.casecmp(file_name) == 0)
        # case sensitive check
        if (base_file == file_name)
          file_to_find = item
          break
        else
          blow_up(file_name, "However, a filename having different capitalization was found: '#{item}'.")
        end
      end
      
    end
    
    if file_to_find.nil?
      case (complain)
        when :error then blow_up(file_name, extra_message) 
        when :warn  then gripe(file_name, extra_message)
        #when :ignore then      
      end
    end
    
    return file_to_find
  end

  private
  
  def blow_up(file_name, extra_message="")
    error = "ERROR: Found no file '#{file_name}' in search paths."
    error += ' ' if (extra_message.length > 0)
    @streaminator.stderr_puts(error + extra_message, Verbosity::ERRORS)
    raise
  end
  
  def gripe(file_name, extra_message="")
    warning = "WARNING: Found no file '#{file_name}' in search paths."
    warning += ' ' if (extra_message.length > 0)
    @streaminator.stderr_puts(warning + extra_message, Verbosity::COMPLAIN)
  end

end


