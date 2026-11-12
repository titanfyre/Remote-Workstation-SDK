cmake_minimum_required(VERSION 3.24)
project(RemoteWorkstationSDK VERSION 0.1.1 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanRemoteWorkstationSDK STATIC
  "sdk/SDK/License.cpp"
  "sdk/SDK/RemoteWorkstation.cpp"
  "sdk/SDK/Transport.cpp"
)
add_library(RemoteWorkstationSDK::sdk ALIAS TitanRemoteWorkstationSDK)
set_target_properties(TitanRemoteWorkstationSDK PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanRemoteWorkstationSDK PUBLIC cxx_std_17)
target_compile_definitions(TitanRemoteWorkstationSDK PUBLIC )
target_compile_definitions(TitanRemoteWorkstationSDK PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanRemoteWorkstationSDK PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanRemoteWorkstationSDK PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/SDK>"
  "$<INSTALL_INTERFACE:include/SDK>"
)
target_link_libraries(TitanRemoteWorkstationSDK PUBLIC advapi32 bcrypt cabinet comctl32 crypt32 gdi32 ncrypt newdev ole32 secur32 setupapi shell32 user32 winhttp wintrust ws2_32)
install(TARGETS TitanRemoteWorkstationSDK EXPORT RemoteWorkstationSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/remote-workstation-sdk)
install(EXPORT RemoteWorkstationSDKTargets NAMESPACE RemoteWorkstationSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RemoteWorkstationSDK)
configure_file("RemoteWorkstationSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/RemoteWorkstationSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/RemoteWorkstationSDKConfigVersion.cmake" VERSION 0.1.1 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/RemoteWorkstationSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/RemoteWorkstationSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RemoteWorkstationSDK)
