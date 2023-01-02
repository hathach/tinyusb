# ==========================================
#   CMock Project - Automatic Mock Generation for C
#   Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
#   [Released under MIT License. Please refer to license.txt for details]
# ==========================================

class CMockGeneratorPluginCallback
  attr_accessor :include_count
  attr_reader :priority
  attr_reader :config, :utils

  def initialize(config, utils)
    @config = config
    @utils = utils
    @priority = 6

    @include_count = @config.callback_include_count
  end

  def instance_structure(function)
    func_name = function[:name]
    "  char #{func_name}_CallbackBool;\n" \
    "  CMOCK_#{func_name}_CALLBACK #{func_name}_CallbackFunctionPointer;\n" \
    "  int #{func_name}_CallbackCalls;\n"
  end

  def mock_function_declarations(function)
    func_name = function[:name]
    return_type = function[:return][:type]
    action = @config.callback_after_arg_check ? 'AddCallback' : 'Stub'
    style  = (@include_count ? 1 : 0) | (function[:args].empty? ? 0 : 2)
    styles = ['void', 'int cmock_num_calls', function[:args_string], "#{function[:args_string]}, int cmock_num_calls"]
    "typedef #{return_type} (* CMOCK_#{func_name}_CALLBACK)(#{styles[style]});\n" \
    "void #{func_name}_AddCallback(CMOCK_#{func_name}_CALLBACK Callback);\n" \
    "void #{func_name}_Stub(CMOCK_#{func_name}_CALLBACK Callback);\n" \
    "#define #{func_name}_StubWithCallback #{func_name}_#{action}\n"
  end

  def generate_call(function)
    args = function[:args].map { |m| m[:name] }
    args << "Mock.#{function[:name]}_CallbackCalls++" if @include_count
    "Mock.#{function[:name]}_CallbackFunctionPointer(#{args.join(', ')})"
  end

  def mock_implementation(function)
    "  if (Mock.#{function[:name]}_CallbackFunctionPointer != NULL)\n  {\n" +
      if function[:return][:void?]
        "    #{generate_call(function)};\n  }\n"
      else
        "    cmock_call_instance->ReturnVal = #{generate_call(function)};\n  }\n"
      end
  end

  def mock_implementation_precheck(function)
    "  if (!Mock.#{function[:name]}_CallbackBool &&\n" \
    "      Mock.#{function[:name]}_CallbackFunctionPointer != NULL)\n  {\n" +
      if function[:return][:void?]
        "    #{generate_call(function)};\n" \
        "    UNITY_CLR_DETAILS();\n" \
        "    return;\n  }\n"
      else
        "    #{function[:return][:type]} cmock_cb_ret = #{generate_call(function)};\n" \
        "    UNITY_CLR_DETAILS();\n" \
        "    return cmock_cb_ret;\n  }\n"
      end
  end

  def mock_interfaces(function)
    func_name = function[:name]
    has_ignore = @config.plugins.include? :ignore
    lines = ''
    lines << "void #{func_name}_AddCallback(CMOCK_#{func_name}_CALLBACK Callback)\n{\n"
    lines << "  Mock.#{func_name}_IgnoreBool = (char)0;\n" if has_ignore
    lines << "  Mock.#{func_name}_CallbackBool = (char)1;\n"
    lines << "  Mock.#{func_name}_CallbackFunctionPointer = Callback;\n}\n\n"
    lines << "void #{func_name}_Stub(CMOCK_#{func_name}_CALLBACK Callback)\n{\n"
    lines << "  Mock.#{func_name}_IgnoreBool = (char)0;\n" if has_ignore
    lines << "  Mock.#{func_name}_CallbackBool = (char)0;\n"
    lines << "  Mock.#{func_name}_CallbackFunctionPointer = Callback;\n}\n\n"
  end

  def mock_verify(function)
    func_name = function[:name]
    "  if (Mock.#{func_name}_CallbackFunctionPointer != NULL)\n  {\n" \
    "    call_instance = CMOCK_GUTS_NONE;\n" \
    "    (void)call_instance;\n  }\n"
  end
end
