cmake_minimum_required(VERSION 3.20)
project(mt5cpp VERSION 0.1.0 LANGUAGES CXX)

option(MT5CPP_BUILD_EXAMPLES "Build mt5cpp examples" ON)

add_library(mt5cpp
  src/mt5cpp/NamedPipeTransport.cpp
  src/mt5cpp/Mt5Client.cpp
)
add_library(mt5cpp::mt5cpp ALIAS mt5cpp)

target_compile_features(mt5cpp PUBLIC cxx_std_17)
target_include_directories(mt5cpp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(WIN32)
  target_compile_definitions(mt5cpp PUBLIC MT5CPP_WINDOWS=1 NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(mt5cpp PUBLIC bcrypt)
else()
  message(FATAL_ERROR "mt5cpp named-pipe transport currently targets Windows only")
endif()

if(MT5CPP_BUILD_EXAMPLES)
  add_executable(mt5cpp_smoke examples/smoke.cpp)
  target_link_libraries(mt5cpp_smoke PRIVATE mt5cpp::mt5cpp)

  add_executable(mt5cpp_stress examples/stress.cpp)
  target_link_libraries(mt5cpp_stress PRIVATE mt5cpp::mt5cpp)

  add_executable(mt5cpp_concurrent examples/concurrent.cpp)
  target_link_libraries(mt5cpp_concurrent PRIVATE mt5cpp::mt5cpp)

  add_executable(mt5cpp_ticks examples/ticks.cpp)
  target_link_libraries(mt5cpp_ticks PRIVATE mt5cpp::mt5cpp)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/mt5cppConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mt5cppConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/mt5cppConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mt5cpp
)

install(TARGETS mt5cpp EXPORT mt5cppTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/mt5cppConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/mt5cppConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mt5cpp
)
install(EXPORT mt5cppTargets NAMESPACE mt5cpp:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mt5cpp)
