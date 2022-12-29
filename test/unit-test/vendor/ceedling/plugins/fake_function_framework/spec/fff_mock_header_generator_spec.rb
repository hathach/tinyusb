require 'stringio'
require 'fff_mock_generator.rb'
require 'header_generator.rb'

# Test the contents of the .h file created for the mock.
describe "FffMockGenerator.create_mock_header" do

  context "when there is nothing to mock," do
    let(:mock_header) {
      parsed_header = {}
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated header file starts with an opening include guard" do
      expect(mock_header).to start_with(
          "#ifndef mock_display_H\n" +
          "#define mock_display_H")
    end
    it "then the generated file ends with a closing include guard" do
      expect(mock_header).to end_with(
          "#endif // mock_display_H\n")
    end
    it "then the generated file includes the fff header" do
      expect(mock_header).to include(
          %{#include "fff.h"\n})
    end
    it "then the generated file has a prototype for the init function" do
      expect(mock_header).to include(
          "void mock_display_Init(void);")
    end
    it "then the generated file has a prototype for the verify function" do
      expect(mock_header).to include(
          "void mock_display_Verify(void);")
    end
    it "then the generated file has a prototype for the destroy function" do
      expect(mock_header).to include(
          "void mock_display_Destroy(void);")
    end
  end

  context "when there is a function with no args and a void return," do
      let(:mock_header) {
        parsed_header = create_cmock_style_parsed_header(
          [{:name => 'display_turnOffStatusLed', :return_type => 'void'}])
        FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
      }
      it "then the generated header file starts with an opening include guard" do
        expect(mock_header).to start_with(
          "#ifndef mock_display_H\n" +
          "#define mock_display_H")
      end
      it "then the generated header file contains a fake function declaration" do
        expect(mock_header).to include(
          "DECLARE_FAKE_VOID_FUNC0(display_turnOffStatusLed);"
        )
      end
      it "then the generated file ends with a closing include guard" do
          expect(mock_header).to end_with(
              "#endif // mock_display_H\n")
      end
  end

  context "when there is a function with no args and a bool return," do
    let(:mock_header) {
      parsed_header = create_cmock_style_parsed_header(
        [{:name => 'display_isError', :return_type => 'bool'}])
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the fake function declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VALUE_FUNC0(bool, display_isError);"
      )
    end
  end

  context "when there is a function with no args and an int return," do
    let(:mock_header) {
      parsed_header = create_cmock_style_parsed_header(
        [{:name => 'display_isError', :return_type => 'int'}])
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the fake function declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VALUE_FUNC0(int, display_isError);"
      )
    end
  end

  context "when there is a function with args and a void return," do
    let(:mock_header) {
      parsed_header = create_cmock_style_parsed_header(
        [{:name => 'display_setVolume', :return_type => 'void', :args => ['int']}])
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the fake function declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VOID_FUNC1(display_setVolume, int);"
      )
    end
  end

  context "when there is a function with args and a value return," do
    let(:mock_header) {
      parsed_header = create_cmock_style_parsed_header(
        [{:name => 'a_function', :return_type => 'int', :args => ['char *']}])
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the fake function declaration" do
      expect(mock_header).to include(
        "FAKE_VALUE_FUNC1(int, a_function, char *);"
      )
    end
  end

  context "when there is a function with many args and a void return," do
    let(:mock_header) {
      parsed_header = create_cmock_style_parsed_header(
        [{:name => 'a_function', :return_type => 'void',
          :args => ['int', 'char *', 'int', 'int', 'bool', 'applesauce']}])
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the fake function declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VOID_FUNC6(a_function, int, char *, int, int, bool, applesauce);"
      )
    end
  end

  context "when there are multiple functions," do
    let(:mock_header) {
      parsed_header = create_cmock_style_parsed_header(
        [ {:name => 'a_function', :return_type => 'int', :args => ['char *']},
          {:name => 'another_function', :return_type => 'void'},
          {:name => 'three', :return_type => 'bool', :args => ['float', 'int']}
        ])
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the first fake function declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VALUE_FUNC1(int, a_function, char *);"
      )
    end
    it "then the generated file contains the second fake function declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VOID_FUNC0(another_function);"
      )
    end
    it "then the generated file contains the third fake function declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VALUE_FUNC2(bool, three, float, int);"
      )
    end
  end

  context "when there is a typedef," do
    let(:mock_header) {
      parsed_header = create_cmock_style_parsed_header(
        nil, ["typedef void (*displayCompleteCallback) (void);"])
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the typedef" do
      expect(mock_header).to include(
        "typedef void (*displayCompleteCallback) (void);"
      )
    end
  end

  context "when there is a void function with variable arguments" do
    let(:mock_header){
      parsed_header = {}
      parsed_header[:functions] = [{
        :name => "function_with_var_args",
        :return => {:type => "void"},
        :var_arg => "...",
        :args => [{:type => 'char *'}]
      }]
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the vararg declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VOID_FUNC2_VARARG(function_with_var_args, char *, ...)"
      )
    end
  end

  context "when there is a function with a return value and variable arguments" do
    let(:mock_header){
      parsed_header = {}
      parsed_header[:functions] = [{
        :name => "function_with_var_args",
        :return => {:type => "int"},
        :var_arg => "...",
        :args => [{:type => 'char *'}]
      }]
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the vararg declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VALUE_FUNC2_VARARG(int, function_with_var_args, char *, ...)"
      )
    end
  end

  context "when there is a void function with variable arguments and " +
          "additional arguments" do
    let(:mock_header){
      parsed_header = {}
      parsed_header[:functions] = [{
        :name => "function_with_var_args",
        :return => {:type => "void"},
        :var_arg => "...",
        :args => [{:type => 'char *'}, {:type => 'int'}]
      }]
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the vararg declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VOID_FUNC3_VARARG(function_with_var_args, char *, int, ...)"
      )
    end
  end

  context "when there is a function with a pointer to a const value" do
    let(:mock_header){
      parsed_header = {}
      parsed_header[:functions] = [{
        :name => "const_test_function",
        :return => {:type => "void"},
        :args => [{:type => "char *", :name => "a", :ptr? => false, :const? => true},
                  {:type => "char *", :name => "b", :ptr? => false, :const? => false}]
      }]
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the correct const argument in the declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VOID_FUNC2(const_test_function, const char *, char *)"
      )
    end
  end

  context "when there is a function that returns a const pointer" do
    let(:mock_header){
      parsed_header = {}
      parsed_header[:functions] = [{
        :name => "return_const_pointer_test_function",
        :modifier => "const",
        :return => {:type => "char *" },
        :args => [{:type => "int", :name => "a"}]
      }]
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the correct const return value in the declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VALUE_FUNC1(const char *, return_const_pointer_test_function, int)"
      )
    end
  end
  
  context "when there is a function that returns a const int" do
    let(:mock_header){
      parsed_header = {}
      parsed_header[:functions] = [{
        :name => "return_const_int_test_function",
        :modifier => "const",
        :return => {:type => "int" },
        :args => []
      }]
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header)
    }
    it "then the generated file contains the correct const return value in the declaration" do
      expect(mock_header).to include(
        "DECLARE_FAKE_VALUE_FUNC0(const int, return_const_int_test_function)"
      )
    end
  end

  context "when there are pre-includes" do
    let(:mock_header) {
      parsed_header = {}
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header,
        [%{"another_header.h"}])
    }
    it "then they are included before the other files" do
      expect(mock_header).to include(
        %{#include "another_header.h"\n} +
        %{#include "fff.h"}
      )
    end
  end

  context "when there are post-includes" do
    let(:mock_header) {
      parsed_header = {}
      FffMockGenerator.create_mock_header("display", "mock_display", parsed_header,
        nil, [%{"another_header.h"}])
    }
    it "then they are included after the other files" do
      expect(mock_header).to include(
        %{#include "display.h"\n} +
        %{#include "another_header.h"\n}
      )
    end
  end

end
