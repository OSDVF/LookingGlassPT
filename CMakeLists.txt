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

configure_file(index.js index.js COPYONLY)
configure_file(package.json package.json COPYONLY)
file(COPY node_modules DESTINATION .)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET LookingGlassPT PROPERTY CXX_STANDARD 20)
endif()
