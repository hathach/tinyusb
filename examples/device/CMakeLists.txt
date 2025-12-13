cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../hw/bsp/family_support.cmake)

project(tinyusb_device_examples C CXX ASM)
family_initialize_project(tinyusb_device_examples ${CMAKE_CURRENT_LIST_DIR})

# family_add_subdirectory will filter what to actually add based on selected FAMILY
set(EXAMPLE_LIST
  audio_4_channel_mic
  audio_4_channel_mic_freertos
  audio_test
  audio_test_freertos
  audio_test_multi_rate
  board_test
  cdc_dual_ports
  cdc_msc
  cdc_msc_freertos
  cdc_uac2
  dfu
  dfu_runtime
  dynamic_configuration
  hid_boot_interface
  hid_composite
  hid_composite_freertos
  hid_generic_inout
  hid_multiple_interface
  midi_test
  midi_test_freertos
  msc_dual_lun
  mtp
  net_lwip_webserver
  uac2_headset
  uac2_speaker_fb
  usbtmc
  video_capture
  video_capture_2ch
  webusb_serial
  )

foreach (example ${EXAMPLE_LIST})
  family_add_subdirectory(${example})
endforeach ()
