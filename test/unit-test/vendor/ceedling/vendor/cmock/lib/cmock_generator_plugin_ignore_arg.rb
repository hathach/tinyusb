class CMockGeneratorPluginIgnoreArg
  attr_reader :priority
  attr_accessor :utils

  def initialize(_config, utils)
    @utils        = utils
    @priority     = 10
  end

  def instance_typedefs(function)
    lines = ''
    function[:args].each do |arg|
      lines << "  char IgnoreArg_#{arg[:name]};\n"
    end
    lines
  end

  def mock_function_declarations(function)
    lines = ''
    function[:args].each do |arg|
      lines << "#define #{function[:name]}_IgnoreArg_#{arg[:name]}()"
      lines << " #{function[:name]}_CMockIgnoreArg_#{arg[:name]}(__LINE__)\n"
      lines << "void #{function[:name]}_CMockIgnoreArg_#{arg[:name]}(UNITY_LINE_TYPE cmock_line);\n"
    end
    lines
  end

  def mock_interfaces(function)
    lines = []
    func_name = function[:name]
    function[:args].each do |arg|
      lines << "void #{func_name}_CMockIgnoreArg_#{arg[:name]}(UNITY_LINE_TYPE cmock_line)\n"
      lines << "{\n"
      lines << "  CMOCK_#{func_name}_CALL_INSTANCE* cmock_call_instance = " \
               "(CMOCK_#{func_name}_CALL_INSTANCE*)CMock_Guts_GetAddressFor(CMock_Guts_MemEndOfChain(Mock.#{func_name}_CallInstance));\n"
      lines << "  UNITY_TEST_ASSERT_NOT_NULL(cmock_call_instance, cmock_line, CMockStringIgnPreExp);\n"
      lines << "  cmock_call_instance->IgnoreArg_#{arg[:name]} = 1;\n"
      lines << "}\n\n"
    end
    lines
  end
end
