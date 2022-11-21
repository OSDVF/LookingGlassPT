# CMakeList.txt : CMake project for LookingGlassPT, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("LookingGlassPT")

# Add source to this project's executable.
add_executable (LookingGlassPT "LookingGlassPT.cpp" "LookingGlassPT.h")

find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(LookingGlassPT
    PRIVATE
    SDL2::SDL2main
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

find_package(GLEW REQUIRED)
target_link_libraries(LookingGlassPT PRIVATE GLEW::GLEW)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET LookingGlassPT PROPERTY CXX_STANDARD 20)
endif()
