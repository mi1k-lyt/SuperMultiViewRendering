cmake_minimum_required(VERSION 3.19.2)
project(SMVR)

list(APPEND dxlibs "")
# Check if in WSL and if has DirectX driver and runtime
if(EXISTS "/usr/lib/wsl/lib/")
  find_library(libd3d12 d3d12 HINTS /usr/lib/wsl/lib)
  find_library(libdxcore dxcore HINTS /usr/lib/wsl/lib)
  list(APPEND dxlibs ${libd3d12} ${libdxcore})
else()
# Fallback to default: let CMake look for libs
  list(APPEND dxlibs d3d12)
  if (MSVC)
    # MINGW doesn't have dxcore yet
    list(APPEND dxlibs dxcore)
  endif()
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dx12/include
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options("/permissive")
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set(VENDOR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SHADER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Resource/Shader)
set(CDX_PATH ${VENDOR_PATH}/dx12)
set(TIFF_PATH ${VENDOR_PATH}/tinyTiff)

add_definitions(-TinyTIFF_BUILD_STATIC_LIBS=ON)

add_subdirectory(${CDX_PATH})
add_subdirectory(${TIFF_PATH})

add_library(STB_IMAGE ${VENDOR_PATH}/stb_image/stb_image.cpp)
set(LIBS ${LIBS} STB_IMAGE)

link_directories(
    ${LIBRARY_OUTPUT_PATH}
)

include_directories(
    ${VENDOR_PATH}
    ${TIFF_PATH}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/build/vendor/tinyTiff/src
)

file(GLOB_RECURSE FILES 
    ${SRC_PATH}/*.cpp
    ${SRC_PATH}/*.h
)

add_executable(${PROJECT_NAME}
    ${FILES}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS")

target_link_libraries(${PROJECT_NAME}
    dxguid
    dxgi
    d3d12
    d3dcompiler
    optimized ${LIBRARY_OUTPUT_PATH}/Release/TinyTIFFShared_Release.lib
    debug ${LIBRARY_OUTPUT_PATH}/Debug/TinyTIFFShared_Debug.lib
    ${LIBS}
)


configure_file(${SRC_PATH}/root_directory.h.in ${SRC_PATH}/root_directory.h)

