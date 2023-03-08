require 'ceedling/plugin'
require 'ceedling/constants'
require 'json'

class CompileCommandsJson < Plugin
  def setup
    @fullpath = File.join(PROJECT_BUILD_ARTIFACTS_ROOT, "compile_commands.json")
    @database = if (File.exists?(@fullpath))
                  JSON.parse( File.read(@fullpath) )
                else
                  []
                end
  end

  def post_compile_execute(arg_hash)

    # Create the new Entry
    value = {
      "directory" => Dir.pwd,
      "command" => arg_hash[:shell_command],
      "file" => arg_hash[:source]
    }

    # Determine if we're updating an existing file description or adding a new one
    index = @database.index {|h| h["file"] == arg_hash[:source]}
    if index
      @database[index] = value
    else
      @database << value
    end

    # Update the Actual compile_commands.json file
    File.open(@fullpath,'w') {|f| f << JSON.pretty_generate(@database)}
  end
end
