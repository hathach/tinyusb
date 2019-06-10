require 'ceedling/constants'
require 'ceedling/file_path_utils'

# create file dependencies to ensure C-based components of vendor tools are recompiled when they are updated with new versions
# forming these explicitly rather than depend on auxiliary dependencies so all scenarios are explicitly covered

file( @ceedling[:file_path_utils].form_test_build_c_object_filepath( UNITY_C_FILE ) => [
  File.join( UNITY_VENDOR_PATH, UNITY_LIB_PATH, UNITY_C_FILE ),
  File.join( UNITY_VENDOR_PATH, UNITY_LIB_PATH, UNITY_H_FILE ),
  File.join( UNITY_VENDOR_PATH, UNITY_LIB_PATH, UNITY_INTERNALS_H_FILE ) ]
  )


if (PROJECT_USE_MOCKS)
file( @ceedling[:file_path_utils].form_test_build_c_object_filepath( CMOCK_C_FILE ) => [
  File.join( CMOCK_VENDOR_PATH, CMOCK_LIB_PATH, CMOCK_C_FILE ),
  File.join( CMOCK_VENDOR_PATH, CMOCK_LIB_PATH, CMOCK_H_FILE ) ]
  )
end


if (PROJECT_USE_EXCEPTIONS)
file( @ceedling[:file_path_utils].form_test_build_c_object_filepath( CEXCEPTION_C_FILE ) => [
  File.join( CEXCEPTION_VENDOR_PATH, CEXCEPTION_LIB_PATH, CEXCEPTION_C_FILE ),
  File.join( CEXCEPTION_VENDOR_PATH, CEXCEPTION_LIB_PATH, CEXCEPTION_H_FILE ) ]
  )
end


if (PROJECT_USE_EXCEPTIONS and PROJECT_RELEASE_BUILD)
file( @ceedling[:file_path_utils].form_release_build_c_object_filepath( CEXCEPTION_C_FILE ) => [
  File.join( CEXCEPTION_VENDOR_PATH, CEXCEPTION_LIB_PATH, CEXCEPTION_C_FILE ),
  File.join( CEXCEPTION_VENDOR_PATH, CEXCEPTION_LIB_PATH, CEXCEPTION_H_FILE ) ]
  )
end
