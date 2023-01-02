# Creates mock files from parsed header files that can be linked into applications.
# The mocks created are compatible with CMock for use with Ceedling.

class FffMockGenerator

  def self.create_mock_header(module_name, mock_name, parsed_header, pre_includes=nil,
    post_includes=nil)
    output = StringIO.new
    write_opening_include_guard(mock_name, output)
    output.puts
    write_extra_includes(pre_includes, output)
    write_header_includes(module_name, output)
    write_extra_includes(post_includes, output)
    output.puts
    write_typedefs(parsed_header, output)
    output.puts
    write_function_declarations(parsed_header, output)
    output.puts
    write_control_function_prototypes(mock_name, output)
    output.puts
    write_closing_include_guard(mock_name, output)
    output.string
  end

  def self.create_mock_source (mock_name, parsed_header, pre_includes=nil,
    post_includes=nil)
    output = StringIO.new
    write_extra_includes(pre_includes, output)
    write_source_includes(mock_name, output)
    write_extra_includes(post_includes, output)
    output.puts
    write_function_definitions(parsed_header, output)
    output.puts
    write_control_function_definitions(mock_name, parsed_header, output)
    output.string
  end

  private

# Header file generation functions.

  def self.write_opening_include_guard(mock_name, output)
    output.puts "#ifndef #{mock_name}_H"
    output.puts "#define #{mock_name}_H"
  end

  def self.write_header_includes(module_name, output)
    output.puts %{#include "fff.h"}
    output.puts %{#include "fff_unity_helper.h"}
    output.puts %{#include "#{module_name}.h"}
  end

  def self.write_typedefs(parsed_header, output)
    return unless parsed_header.key?(:typedefs)
    parsed_header[:typedefs].each do |typedef|
      output.puts typedef
    end
  end

  def self.write_function_declarations(parsed_header, output)
    write_function_macros("DECLARE", parsed_header, output)
  end


  def self.write_control_function_prototypes(mock_name, output)
    output.puts "void #{mock_name}_Init(void);"
    output.puts "void #{mock_name}_Verify(void);"
    output.puts "void #{mock_name}_Destroy(void);"
  end

  def self.write_closing_include_guard(mock_name, output)
    output.puts "#endif // #{mock_name}_H"
  end

# Source file generation functions.

  def self.write_source_includes (mock_name, output)
    output.puts "#include <string.h>"
    output.puts %{#include "fff.h"}
    output.puts %{#include "#{mock_name}.h"}
  end

  def self.write_function_definitions(parsed_header, output)
    write_function_macros("DEFINE", parsed_header, output)
  end

  def self.write_control_function_definitions(mock_name, parsed_header, output)
    output.puts "void #{mock_name}_Init(void)"
    output.puts "{"
    # In the init function, reset the FFF globals. These are used for things
    # like the call history.
    output.puts "    FFF_RESET_HISTORY();"
    
    # Also, reset all of the fakes.
    if parsed_header[:functions]
      parsed_header[:functions].each do |function|
        output.puts "    RESET_FAKE(#{function[:name]})"
      end
    end
    output.puts "}"
    output.puts "void #{mock_name}_Verify(void)"
    output.puts "{"
    output.puts "}"
    output.puts "void #{mock_name}_Destroy(void)"
    output.puts "{"
    output.puts "}"
  end

# Shared functions.

  def self.write_extra_includes(includes, output)
    if includes
      includes.each {|inc| output.puts "#include #{inc}\n"}
    end
  end

  def self.write_function_macros(macro_type, parsed_header, output)
    return unless parsed_header.key?(:functions)
    parsed_header[:functions].each do |function|
      name = function[:name]
      return_type = function[:return][:type]
      if function.has_key? :modifier
          # Prepend any modifier. If there isn't one, trim any leading whitespace.
          return_type = "#{function[:modifier]} #{return_type}".lstrip
      end
      arg_count = function[:args].size

      # Check for variable arguments.
      var_arg_suffix = ""
      if function[:var_arg]
        # If there are are variable arguments, then we need to add this argument
        # to the count, update the suffix that will get added to the macro.
        arg_count += 1
        var_arg_suffix = "_VARARG"
      end

      # Generate the correct macro.
      if return_type == 'void'
        output.print "#{macro_type}_FAKE_VOID_FUNC#{arg_count}#{var_arg_suffix}(#{name}"
      else
        output.print "#{macro_type}_FAKE_VALUE_FUNC#{arg_count}#{var_arg_suffix}(#{return_type}, #{name}"
      end

      # Append each argument type.
      function[:args].each do |arg|
        output.print ", "
        if arg[:const?]
          output.print "const "
        end
        output.print "#{arg[:type]}"
      end

      # If this argument list ends with a variable argument, add it here at the end.
      if function[:var_arg]
        output.print ", ..."
      end

      # Close the declaration.
      output.puts ");"
    end
  end

end
