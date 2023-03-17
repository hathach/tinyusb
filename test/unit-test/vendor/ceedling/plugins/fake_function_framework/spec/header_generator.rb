# Create a CMock-style parsed header hash. This the type of hash created by
# CMock when parsing header files for automock generation. It contains all of
# includes, typedefs and functions (with return types and arguments) parsed from
# the header file.
def create_cmock_style_parsed_header(functions, typedefs = nil)
  parsed_header = {
      :includes => nil,
      :functions => [],
      :typedefs => []
  }

  # Add the typedefs.
  if typedefs
      typedefs.each do |typedef|
          parsed_header[:typedefs] << typedef
      end
  end

  # Add the functions.
  if functions
    functions.each do |function|
      # Build the array of arguments.
      args = []
      if function.key?(:args)
        function[:args].each do |arg|
          args << {
            :type => arg
          }
        end
      end
      parsed_header[:functions] << {
        :name => function[:name],
        :modifier => "",
        :return => {
          :type => function[:return_type],
          :name => "cmock_to_return",
          :ptr? => false,
          :const? => false,
          :str => "void cmock_to_return",
          :void? => true
        },
        :var_arg => nil,
        :args_string => "void",
        :args => args,
        :args_call => "",
        :contains_ptr? => false
      }
    end
  end
  parsed_header
end
