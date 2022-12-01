# CMakeList.txt : CMake project for LookingGlassPT, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("LookingGlassPT")

file(GLOB all_SRCS
        "${PROJECT_SOURCE_DIR}/impl/*.h"
        "${PROJECT_SOURCE_DIR}/impl/*.hpp"
        "${PROJECT_SOURCE_DIR}/impl/*.cpp"
        "${PROJECT_SOURCE_DIR}/impl/*.c"
        "${PROJECT_SOURCE_DIR}/*.c"
        "${PROJECT_SOURCE_DIR}/*.cpp"
        "${PROJECT_SOURCE_DIR}/*.h"
        "${PROJECT_SOURCE_DIR}/*.hpp"
        )

# Add source to this project's executable.
add_executable (LookingGlassPT ${all_SRCS})

find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(LookingGlassPT
    PRIVATE
    SDL2::SDL2main
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

find_package(GLEW REQUIRED)
target_link_libraries(LookingGlassPT PRIVATE GLEW::GLEW)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(LookingGlassPT PRIVATE imgui::imgui)

find_package(libusb CONFIG REQUIRED)
target_include_directories(LookingGlassPT PRIVATE ${LIBUSB_INCLUDE_DIRS})
target_link_libraries(LookingGlassPT PRIVATE ${LIBUSB_LIBRARIES})

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(LookingGlassPT PRIVATE nlohmann_json::nlohmann_json)

set(nlohmann-json_IMPLICIT_CONVERSIONS OFF)

find_package(tiny-process-library CONFIG REQUIRED)
target_link_libraries(LookingGlassPT PRIVATE tiny-process-library::tiny-process-library)

# 
# Include node.js program in the compilation
# depends on npm
#
configure_file(index.js index.js COPYONLY)
configure_file(package.json package.json COPYONLY)

add_custom_command(OUTPUT node_modules COMMAND npm install) # install dependencies
add_custom_target(nodejs_program ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/node_modules) 
add_dependencies(LookingGlassPT nodejs_program)

set(SHADERS vertex.vert fragment.frag)
foreach(CurrentShader IN LISTS SHADERS)
    add_custom_command(OUTPUT ${CurrentShader}
        MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/${CurrentShader}
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/${CurrentShader} ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_target("${CurrentShader}target" ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${CurrentShader}) 
    add_dependencies(LookingGlassPT "${CurrentShader}target")
endforeach()

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET LookingGlassPT PROPERTY CXX_STANDARD 20)
endif()

if(ASAN_ENABLED)
  if(MSVC)
    target_compile_options(LookingGlassPT PUBLIC /fsanitize=address)
  else()
    target_compile_options(LookingGlassPT PUBLIC -fsanitize=address /wholearchive: clang_rt.asan_dbg_dynamic-x86_64.lib clang_rt.asan_dbg_dynamic_runtime_thunk-x86_64.lib)
    target_link_options(LookingGlassPT PUBLIC -fsanitize=address /wholearchive: clang_rt.asan_dbg_dynamic-x86_64.lib clang_rt.asan_dbg_dynamic_runtime_thunk-x86_64.lib)
  endif()
endif()

target_sources(LookingGlassPT PRIVATE dpiAware.manifest)


if (WIN32)
    # /ENTRY:mainCRTStartup keeps the same "main" function instead of requiring "WinMain"
    set(SUBSYSTEM_LINKER_OPTIONS "/SUBSYSTEM:CONSOLE")
else()
    set(SUBSYSTEM_LINKER_OPTIONS "-mwindows")
endif()

target_link_options(LookingGlassPT PRIVATE ${SUBSYSTEM_LINKER_OPTIONS})