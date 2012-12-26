require 'rubygems'
require 'rake' # for ext()
require 'fileutils'
require 'constants'


# :flags:
#   :release:
#     :compile:
#       :*:          # add '-foo' to compilation of all files not main.c
#         - -foo
#       :main:       # add '-Wall' to compilation of main.c
#         - -Wall
#   :test:
#     :link:
#       :test_main:  # add '--bar --baz' to linking of test_main.exe
#         - --bar
#         - --baz


class Flaginator

  constructor :configurator

  def flag_down( operation, context, file )
    # create configurator accessor method
    accessor = ('flags_' + context.to_s).to_sym
    
    # create simple filename key from whatever filename provided
    file_key = File.basename( file ).ext('').to_sym
    
    # if no entry in configuration for flags for this context, bail out
    return [] if not @configurator.respond_to?( accessor )
    
    # get flags sub hash associated with this context
    flags = @configurator.send( accessor )

    # if operation not represented in flags hash, bail out
    return [] if not flags.include?( operation )
    
    # redefine flags to sub hash associated with the operation
    flags = flags[operation]
    
    # if our file is in the flags hash, extract the array of flags
    if (flags.include?( file_key )) then return flags[file_key]
    # if our file isn't in the flags hash, but there is default for all other files, extract array of flags
    elsif (flags.include?( :* )) then return flags[:*]
    end

    # fall through: flags were specified but none applying to present file
    return []
  end

end
