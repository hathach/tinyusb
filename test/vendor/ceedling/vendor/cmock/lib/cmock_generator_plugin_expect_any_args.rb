# ==========================================
#   CMock Project - Automatic Mock Generation for C
#   Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
#   [Released under MIT License. Please refer to license.txt for details]
# ==========================================

class CMockGeneratorPluginExpectAnyArgs

  attr_reader :priority
  attr_reader :config, :utils

  def initialize(config, utils)
    @config = config
    @utils = utils
    @priority = 3
  end

  def instance_typedefs(function)
    "  int ExpectAnyArgsBool;\n"
  end

  def mock_function_declarations(function)
    unless (function[:args].empty?)
      if (function[:return][:void?])
        return "#define #{function[:name]}_ExpectAnyArgs() #{function[:name]}_CMockExpectAnyArgs(__LINE__)\n" +
               "void #{function[:name]}_CMockExpectAnyArgs(UNITY_LINE_TYPE cmock_line);\n"
      else
        return "#define #{function[:name]}_ExpectAnyArgsAndReturn(cmock_retval) #{function[:name]}_CMockExpectAnyArgsAndReturn(__LINE__, cmock_retval)\n" +
               "void #{function[:name]}_CMockExpectAnyArgsAndReturn(UNITY_LINE_TYPE cmock_line, #{function[:return][:str]});\n"
      end
    else
      ""
    end
  end

  def mock_interfaces(function)
    lines = ""
    unless (function[:args].empty?)
      if (function[:return][:void?])
        lines << "void #{function[:name]}_CMockExpectAnyArgs(UNITY_LINE_TYPE cmock_line)\n{\n"
      else
        lines << "void #{function[:name]}_CMockExpectAnyArgsAndReturn(UNITY_LINE_TYPE cmock_line, #{function[:return][:str]})\n{\n"
      end
      lines << @utils.code_add_base_expectation(function[:name], true)
      unless (function[:return][:void?])
        lines << "  cmock_call_instance->ReturnVal = cmock_to_return;\n"
      end
      lines << "  cmock_call_instance->ExpectAnyArgsBool = (int)1;\n"
      lines << "}\n\n"
    end
    return lines
  end
end
