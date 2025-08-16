project(score_addon_lsl LANGUAGES CXX)

# Find LSL
block()
  set(CMAKE_UNITY_BUILD 0)
  set(LSL_BUILD_STATIC 1)
  set(LSL_INSTALL 0)
  set(LSL_ADDITIONAL_TARGETS 0)
  set(LSL_OPTIMIZATIONS 0)
  set(LSL_BUNDLED_BOOST 0)
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
