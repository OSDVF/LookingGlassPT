# Path Tracer for Looking Glass display
> Multiwindow multithreaded scene rendering example.  
*Shoot rays through your Looking Glass.*

Yet another quick school project.

## Features
- Automatic Looking Glass calibration
	- through Holoplay.js if you have Looking Glass Bridge application running
	- through good old USB interface as a fallback
- Loading scenes (many supported formats through [assimp](https://github.com/assimp/assimp) but optimized for glTF with no embedded textures)
- Non-realtime path tracing
- Realtime ray tracing
- Input processing on one thread and rendering on another thread (event queue synchronization may be slow, but VS profiler shows 'not that much'🙃)
- Works even if you set display scaling different than 100% (like me)

## Build
- There was the aim to make this project portable, but it was tested only on Windows 10 x64 with GTX 1050 Mobile
- Install [CMake](https://cmake.org/), [vcpkg](https://vcpkg.io/) and [node](https://nodejs.org/en/).
- Setup `vcpkg` by running
	- `.\vcpkg\bootstrap-vcpkg.bat` or `.\vcpkg\bootstrap-vcpkg.sh` in the directory where you cloned `vcpkg`
	- this will also print your toolchain file path `something.../vcpkg.cmake`
	- If you are using Visual Studio, you can also `vcpkg integrate install`
- clone this repo `git clone git@github.com:OSDVF/LookingGlassPT.git`
- `cd LookingGlassPT`
- `mkdir build && cmake -B./build -S. -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake`
- or for the Visual Studio: edit `CMakePresets.json` and change `C:/vcpkg/scripts/buildsystems/vcpkg.cmake` to your path

## Run
There are two windows
- Control window for changing program settings variables (e.g. switching between flat screen and looking glass)
- Output window
Optional program arguments
- `d` display debug console
- `flat` skip initial the searching for a connected looking glass and use flat screen directly

## Dependencies
- C++ 20
- System with OpenGL 4.3 and bindless texture support (ARB_bindless_texture) ([find your GPU support here](https://opengl.gpuinfo.org/))
- [CMake](https://cmake.org/)
- [vcpkg](https://vcpkg.io/)
- [node](https://nodejs.org/en/) - only for calibration via [Holoplay.js](https://www.npmjs.com/package/holoplay)
- node modules (managed automatically by `npm`)
	- holoplay
	- mock-dom-resources
	- three
	- ws

- C++ libraries (managed automatically by `vcpkg`)
	- `glew`
	- `glm`
	- `sdl2`
	- `imgui`
	- `libusb` - for calibration via USB
	- `nlohmann-json` - for parsing calibration data
	- `tiny-process-library`
	- `assimp`
	- `stb` - for loading scene textures 
	- `nativefiledialog` - for the selection of scene file
	- `boost-stacktrace` - for nice debug info

# Thanks to
- [Department of Computer Graphics and Multimedia at FIT BUT](https://www.fit.vut.cz/units/upgm/.en) - for borrowing me a Looking Glass display and ray generation
- [3DApps](https://github.com/dormon/3DApps) - LG USB Calibration
- [Embree-style Möller-Trumbore ray-triangle intersection](https://www.shadertoy.com/view/llGSDD)
- [Scene loading](https://github.com/assimp/assimp/blob/master/samples/SimpleTexturedOpenGL/SimpleTexturedOpenGL/src/model_loading.cpp) - assimp example
- [Tutorial about bindless textures](https://sites.google.com/site/john87connor/indirect-rendering/2-a-using-bindless-textures?pli=1)
