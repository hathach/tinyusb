# ==========================================
#   CMock Project - Automatic Mock Generation for C
#   Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
#   [Released under MIT License. Please refer to license.txt for details]
# ========================================== 

class CMockGeneratorPluginIgnore

  attr_reader :priority
  attr_reader :config, :utils
  
  def initialize(config, utils)
    @config = config
    if (@config.ignore == :args_and_calls)
      alias :mock_implementation_precheck :mock_implementation_for_ignores
      alias :mock_implementation          :nothing
      alias :mock_verify                  :mock_conditionally_verify_counts
    else
      alias :mock_implementation          :mock_implementation_for_ignores
      alias :mock_implementation_precheck :nothing
      alias :mock_verify                  :nothing
    end
    @utils = utils
    @priority = 2
  end
  
  def instance_structure(function)
    if (function[:return][:void?])
      "  int #{function[:name]}_IgnoreBool;\n"
    else
      "  int #{function[:name]}_IgnoreBool;\n  #{function[:return][:type]} #{function[:name]}_FinalReturn;\n"
    end
  end
  
  def mock_function_declarations(function)
    if (function[:return][:void?])
      if (@config.ignore == :args_only)
        return "#define #{function[:name]}_Ignore() #{function[:name]}_CMockIgnore(__LINE__)\n" +
               "void #{function[:name]}_CMockIgnore(UNITY_LINE_TYPE cmock_line);\n"
      else
        return "#define #{function[:name]}_Ignore() #{function[:name]}_CMockIgnore()\n" +
               "void #{function[:name]}_CMockIgnore(void);\n"
      end
    else        
      return "#define #{function[:name]}_IgnoreAndReturn(cmock_retval) #{function[:name]}_CMockIgnoreAndReturn(__LINE__, cmock_retval)\n" +
             "void #{function[:name]}_CMockIgnoreAndReturn(UNITY_LINE_TYPE cmock_line, #{function[:return][:str]});\n"
    end 
  end
  
  def mock_implementation_for_ignores(function)
    lines = "  if (Mock.#{function[:name]}_IgnoreBool)\n  {\n" 
    if (function[:return][:void?])
      lines << "    return;\n  }\n"
    else
      retval = function[:return].merge( { :name => "cmock_call_instance->ReturnVal"} )
      lines << "    if (cmock_call_instance == NULL)\n      return Mock.#{function[:name]}_FinalReturn;\n"
      lines << "  " + @utils.code_assign_argument_quickly("Mock.#{function[:name]}_FinalReturn", retval) unless (retval[:void?])
      lines << "    return cmock_call_instance->ReturnVal;\n  }\n"
    end
    lines
  end
  
  def mock_interfaces(function)
    lines = ""
    args_only = (@config.ignore == :args_only)
    if (function[:return][:void?])
      if (args_only)
        lines << "void #{function[:name]}_CMockIgnore(UNITY_LINE_TYPE cmock_line)\n{\n"
      else
        lines << "void #{function[:name]}_CMockIgnore(void)\n{\n"
      end
    else
      lines << "void #{function[:name]}_CMockIgnoreAndReturn(UNITY_LINE_TYPE cmock_line, #{function[:return][:str]})\n{\n"
    end
    if (args_only)
      lines << @utils.code_add_base_expectation(function[:name], true)
    elsif (!function[:return][:void?]) 
      lines << @utils.code_add_base_expectation(function[:name], false)
    end
    unless (function[:return][:void?])
      lines << "  cmock_call_instance->ReturnVal = cmock_to_return;\n"
    end
    lines << "  Mock.#{function[:name]}_IgnoreBool = (int)1;\n"
    lines << "}\n\n"
  end

  def mock_conditionally_verify_counts(function)
    func_name = function[:name]
    "  if (Mock.#{func_name}_IgnoreBool)\n    Mock.#{func_name}_CallInstance = CMOCK_GUTS_NONE;\n"
  end
  
  def nothing(function)
    return ""
  end
end
