
class ReportinatorHelper

    # Output the shell result to the console.
    def print_shell_result(shell_result)
      if !(shell_result.nil?)
        puts "Done in %.3f seconds." % shell_result[:time]
  
        if !(shell_result[:output].nil?) && (shell_result[:output].length > 0)
          puts shell_result[:output]
        end
      end
    end

end
