# demo-02_displayxr 
A basic rendering demo using [xrlib](https://github.com/1runeberg/xrlib) that shows the fundamental setup needed for XR visualization. This demo covers:

- Basic XR session setup
- Render loop initialization
- Simple geometry rendering
- XR space and pose handling
- View and projection setup

For build instructions, see the [xrlib demos build guide](https://github.com/1runeberg/xrlib-demos)

## visionOS

Requires CMake 3.28 or newer, Xcode with the visionOS SDK and `glslc` from the
[Vulkan SDK](https://vulkan.lunarg.com/). Clone the [public SDK](https://github.com/1runeberg/vision-openxr-sdk) as `vision-openxr-sdk`
alongside xrlib-demos.

From the xrlib-demos root:

```sh
git -C ../vision-openxr-sdk submodule update --init --recursive
cmake -S demo-02_displayxr -B demo-02_displayxr/build/xcode -G Xcode \
    -DCMAKE_SYSTEM_NAME=visionOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=27.0 \
    -DVISION_OPENXR_SDK="$PWD/../vision-openxr-sdk" \
    -DAPPLE_TEAM=YOUR_TEAM_ID
cmake --build demo-02_displayxr/build/xcode --config Debug
```

CMake generates `build/xcode/02_displayxr.xcodeproj` under this demo.
Open it in Xcode and select the `displayxr` scheme to run on Vision Pro,
or use the command above to build `build/xcode/Debug/DisplayXR.app`.
Omit `APPLE_TEAM` for an unsigned build. Requires visionOS 27 or later.

To configure through the demos root instead, use `-S .` and add
`-DVISIONOS_PROJECT=demo-02_displayxr` to the same command. The generated project
is then `xrlib_demos.xcodeproj` in the build folder, with the same scheme and app output.

The visionOS config lives in `visionos/CMakeLists.txt`. CMake builds xrlib,
embeds the SDK frameworks and recompiles shaders when they change.
The floor and axis markers use OpenXR `LOCAL_FLOOR`.
