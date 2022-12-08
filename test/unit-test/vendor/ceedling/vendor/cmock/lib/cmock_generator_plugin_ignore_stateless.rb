# ==========================================
#   CMock Project - Automatic Mock Generation for C
#   Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
#   [Released under MIT License. Please refer to license.txt for details]
# ==========================================

class CMockGeneratorPluginIgnoreStateless
  attr_reader :priority
  attr_reader :config, :utils

  def initialize(config, utils)
    @config = config
    @utils = utils
    @priority = 2
  end

  def instance_structure(function)
    if function[:return][:void?]
      "  char #{function[:name]}_IgnoreBool;\n"
    else
      "  char #{function[:name]}_IgnoreBool;\n  #{function[:return][:type]} #{function[:name]}_FinalReturn;\n"
    end
  end

  def mock_function_declarations(function)
    lines = if function[:return][:void?]
              "#define #{function[:name]}_Ignore() #{function[:name]}_CMockIgnore()\n" \
              "void #{function[:name]}_CMockIgnore(void);\n"
            else
              "#define #{function[:name]}_IgnoreAndReturn(cmock_retval) #{function[:name]}_CMockIgnoreAndReturn(cmock_retval)\n" \
              "void #{function[:name]}_CMockIgnoreAndReturn(#{function[:return][:str]});\n"
            end

    # Add stop ignore function. it does not matter if there are any args
    lines << "#define #{function[:name]}_StopIgnore() #{function[:name]}_CMockStopIgnore()\n" \
                "void #{function[:name]}_CMockStopIgnore(void);\n"
    lines
  end

  def mock_implementation_precheck(function)
    lines = "  if (Mock.#{function[:name]}_IgnoreBool)\n  {\n"
    lines << "    UNITY_CLR_DETAILS();\n"
    if function[:return][:void?]
      lines << "    return;\n  }\n"
    else
      retval = function[:return].merge(:name => 'cmock_call_instance->ReturnVal')
      lines << "    if (cmock_call_instance == NULL)\n      return Mock.#{function[:name]}_FinalReturn;\n"
      lines << '  ' + @utils.code_assign_argument_quickly("Mock.#{function[:name]}_FinalReturn", retval) unless retval[:void?]
      lines << "    return cmock_call_instance->ReturnVal;\n  }\n"
    end
    lines
  end

  # this function is adjusted
  def mock_interfaces(function)
    lines = ''
    lines << if function[:return][:void?]
               "void #{function[:name]}_CMockIgnore(void)\n{\n"
             else
               "void #{function[:name]}_CMockIgnoreAndReturn(#{function[:return][:str]})\n{\n"
             end
    unless function[:return][:void?]
      lines << "  Mock.#{function[:name]}_CallInstance = CMOCK_GUTS_NONE;\n"
      lines << "  Mock.#{function[:name]}_FinalReturn = cmock_to_return;\n"
    end
    lines << "  Mock.#{function[:name]}_IgnoreBool = (char)1;\n"
    lines << "}\n\n"

    # Add stop ignore function. it does not matter if there are any args
    lines << "void #{function[:name]}_CMockStopIgnore(void)\n{\n"
    lines << "  Mock.#{function[:name]}_IgnoreBool = (char)0;\n"
    lines << "}\n\n"

    lines
  end

  def mock_ignore(function)
    "  Mock.#{function[:name]}_IgnoreBool = (char)1;\n"
  end

  def mock_verify(function)
    func_name = function[:name]
    "  if (Mock.#{func_name}_IgnoreBool)\n    call_instance = CMOCK_GUTS_NONE;\n"
  end
end
