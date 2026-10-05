# demo-06_gltfxr

[<img src="../images/demo_06_gltfxr_thumb.png" alt="gltfxr" width="200" />](https://youtu.be/dvE7YCwCSKQ)

A passthrough demo using [xrlib](https://github.com/1runeberg/xrlib) and its optional renderer(xrvk) to load glTF sample models with PBR materials and selectable animations.

- Shared OpenXR scene for Apple Vision Pro and Meta Quest
- visionOS uses the public [vision-openxr SDK](https://github.com/1runeberg/vision-openxr-sdk), Meta uses `XR_FB_passthrough`
- Prepared ASTC textures with mipmaps and an on-screen loading log
- Models (from Khronos samples, Valve and Sketchfab - licence notices in the respective asset folders):
  - Damaged helmet with base colour, metallic/roughness, normal, emissive and occlusion maps
  - Fox and BrainStem with vertex shader skinning
  - Flight Helmet for heavier texture loading
  - SteamVR glove with skinned hand animations
  - Saber hilt from [inputxr](../demo-05_inputxr) for PBR comparison

## Mechanics

The model and panels appear in front of you at head height, seated or standing. On Vision Pro, allow world sensing for floor detection when prompted:

- Damaged Helmet loads first. Pick another model from the left panel
- The loading ring shows progress. The console on the right shows loading stages, errors and timings
- Animated models start with their first clip. Pick a clip to play it. Stop resets it to the first frame, Play restarts it
- Pinch and drag to scroll a list, or use the arrows. Models and Animations scroll separately
- Controls highlight on hover, using system hover on Vision Pro or the controller aim ray
- Info prints the model's glTF metadata to the console
- Immersive switches to the night grid and its lighting. Passthrough returns to studio lighting
- The chart button shows frame timings, CPU time, wait time and missed frames in the console panel
- Exit finishes pending loading work and closes the app


## Build for visionOS

Requires CMake 3.28 or newer, Xcode with the visionOS SDK and `glslc` from the
[Vulkan SDK](https://vulkan.lunarg.com/). Clone the [public SDK](https://github.com/1runeberg/vision-openxr-sdk) as `vision-openxr-sdk` alongside xrlib-demos.

From the xrlib-demos root:

```sh
git -C ../vision-openxr-sdk submodule update --init --recursive
cmake -S demo-06_gltfxr -B demo-06_gltfxr/build/xcode -G Xcode \
    -DCMAKE_SYSTEM_NAME=visionOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=27.0 \
    -DVISION_OPENXR_SDK="$PWD/../vision-openxr-sdk" \
    -DAPPLE_TEAM=YOUR_TEAM_ID
cmake --build demo-06_gltfxr/build/xcode --config Release
```

CMake generates `build/xcode/06_gltfxr.xcodeproj` under this demo. Open it in Xcode and
select the `gltfxr` scheme to run on Vision Pro, or use the command above to build
`build/xcode/Release/GltfXR.app`.

Replace `YOUR_TEAM_ID` with your Apple development team ID. Omit `APPLE_TEAM` for an
unsigned build. Requires visionOS 27 or later.

To configure through the demos root instead, use `-S .` and add `-DVISIONOS_PROJECT=demo-06_gltfxr` to the same command.

## Build for Meta Quest

Follow the Android setup in the [xrlib demos build guide](https://github.com/1runeberg/xrlib-demos), then from this demo:

```sh
cd android
./gradlew assembleDebug
```

You'll find the APK in `android/build/outputs/apk/debug/`.

Meta device rendering still needs visual validation.

## Known Issues

- On visionOS, the app exits after host cleanup so each launch starts fresh. Resuming a suspended instance can leave pinch input unresponsive
- Foveated rendering isn't supported on visionOS yet
- System hover can cause missed frames on visionOS, particularly in immersive mode


## Asset preparation

Prepared models and lighting are bundled in the `assets/bin/Prepared*` folders, so app builds just package them. Rebuild them if you change or add models.

Textures are prepared as ASTC KTX2 mip chains by `xrvk-tools gltf --encode astc`, since gltfxr only runs on mobile GPUs. Normal maps use smaller blocks to stay sharp, and 16-bit sources stay uncompressed. The studio lighting is baked by `xrvk-tools environment` and the night grid's lighting and sky by `xrvk-tools night-grid`.

The asset build compiles `xrvk-tools` from xrlib's `tools/xrvk` for your build host, so you'll need a host C++20 compiler, the [Vulkan SDK](https://vulkan.lunarg.com/) headers and the `ktx` tool from [KTX-Software](https://github.com/KhronosGroup/KTX-Software/releases). Put `ktx` on PATH or pass `-DKTX_EXECUTABLE=/absolute/path/to/ktx`.

From this demo:

```sh
cmake -S assets -B build/assets
cmake --build build/assets
```

Textures regenerate when inputs or tools change, or outputs are missing. Build the `regenerate_assets` target to force regeneration, or `regenerate_environment` to rebake the studio and night grid lighting.

Images shared across sRGB and linear slots, or incompatible wrap modes, need separate source images.

The panel shaders in `assets/src` are compiled into `assets/bin` by a desktop build of the demos.
