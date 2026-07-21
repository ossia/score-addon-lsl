cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

if(NOT TARGET score_lib_base)
  include(ScoreExternalAddon)
endif()

project(score_addon_lsl LANGUAGES CXX)

# Find LSL
block()
  set(CMAKE_UNITY_BUILD 0)
  set(LSL_BUILD_STATIC 1)
  set(LSL_INSTALL 0)
  set(LSL_ADDITIONAL_TARGETS 0)
  set(LSL_OPTIMIZATIONS 0)
  set(LSL_BUNDLED_BOOST 0)

  # Target the same Windows version as the rest of score.
  #
  # liblsl defaults LSL_WINVER to 0x0601 and applies it as _WIN32_WINNT to its
  # targets, so without this its translation units are built for Windows 7
  # while everything around them is built for Windows 10. That is not just a
  # stale floor: Asio reads _WIN32_WINNT to decide what it may use, notably
  # ASIO_HAS_STD_ATOMIC_WAIT, so the standalone Asio bundled here ends up
  # configured differently from the Boost.Asio in libossia even though the two
  # meet in the same binary.
  set(LSL_WINVER "0x0A00")

  add_subdirectory(3rdparty/liblsl)
endblock()

set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/LSLDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/LSLProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/LSLProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/LSLSpecificSettings.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/lsl_protocol.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/lsl_context.hpp"
  
  "${CMAKE_CURRENT_SOURCE_DIR}/score_addon_lsl.hpp"
)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/LSLDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/LSLProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/LSLProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/LSLSpecificSettingsSerialization.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/lsl_protocol.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LSL/lsl_context.cpp"
  
  "${CMAKE_CURRENT_SOURCE_DIR}/score_addon_lsl.cpp"
)

# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC 
  score_plugin_deviceexplorer
  LSL::lsl
)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
