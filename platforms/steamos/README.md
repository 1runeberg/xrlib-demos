# SteamOS ARM64

Builds checkxr, displayxr and inputxr as native Linux ARM64 apps for Steam Frame.
Uses Valve's Sniper SDK and packages each demo for the SteamOS Devkit Client.
InputXR uses xrlib's Steam Frame controller profile when the runtime supports
`XR_VALVE_frame_controller_interaction`. Other runtimes keep their existing
controller profiles.

## Build with Docker

Requires Git and Docker with Linux ARM64 support. Docker Desktop provides this on Apple Silicon Macs and Windows x86-64.

Linux hosts need Docker Engine with BuildKit, and x86-64 hosts also need ARM64 QEMU/binfmt support. Builds on x86-64
use emulation and take longer.

From the repo root:

```sh
git submodule update --init --recursive
docker build --platform linux/arm64 --target packages \
    --output type=local,dest=. \
    -f platforms/steamos/Dockerfile .
```

In PowerShell, run the Docker command on one line. The image supplies the SDK,
CMake and shader compiler. It configures each demo through the repo root,
installs the packages and exports them to the host. The first build downloads and builds the tools, later
builds reuse Docker's cache. Packages are exported into each demo's build folder.

To build one demo, add `--build-arg DEMO=demo-02_displayxr`. Add
`--build-arg BUILD_ENTRY=standalone` to configure directly from the demo.

The GitHub **SteamOS ARM64** workflow uses the same Dockerfile on an ARM64
runner and uploads the deployment archive.

## Build directly on Linux ARM64

Requires a Linux ARM64 environment compatible with the
[Steam Linux Runtime 3.0 (Sniper) SDK](https://gitlab.steamos.cloud/steamrt/sniper/sdk),
CMake 3.28 or newer, a C++20 compiler, Vulkan development libraries and `glslc`.
Initialise the submodules, then run from the repo root:

```sh
git submodule update --init --recursive
cmake -S demo-02_displayxr -B demo-02_displayxr/build/steamos-arm64 \
    -DSTEAMOS_ARM64=ON -DCMAKE_BUILD_TYPE=Release
cmake --build demo-02_displayxr/build/steamos-arm64 --parallel
cmake --install demo-02_displayxr/build/steamos-arm64 --component SteamOS
```

To configure through the repo root instead, use `-S .` and add
`-DSTEAMOS_PROJECT=demo-02_displayxr`. Both entry points use the same output
folder. Use a fresh build folder when switching entry points.

Replace the demo folder to build checkxr or inputxr. Set
`-DCMAKE_BUILD_TYPE=Debug` for a debug build.

## Packages

Each demo keeps its CMake build files and binaries under
`<demo>/build/steamos-arm64`. Installation collects the executable, libraries
and assets in its `deploy` subfolder:

- `demo-01_checkxr/build/steamos-arm64/deploy`: OpenXR runtime queries
- `demo-02_displayxr/build/steamos-arm64/deploy`: stereo rendering
- `demo-05_inputxr/build/steamos-arm64/deploy`: controller input and haptics

Each folder includes a `launch.sh`, executable, xrlib and the assets it needs.
The OpenXR loader is embedded in xrlib. Vulkan and the OpenXR runtime come from
the device. The launcher sets the working directory for relative asset paths.

## Deploy to Frame

1. Enable **Settings > System > Developer Mode** on Frame
2. Select **Settings > Developer > Pair new host**
3. Register the headset in SteamOS Devkit Client and confirm pairing on Frame
4. Open **Title Upload** and select one demo's deployment folder as **Local Folder**
5. Set **Start Command** to `./launch.sh`
6. Select **Steam Linux Runtime 3.0 ARM64 (Sniper)** as the runtime
7. Click **Upload**, then **Start**

The uploaded demo also appears under **Library > Non-Steam** on Frame.
Checkxr writes its results to stdout and exits, it doesn't display an XR scene.

See Valve's [deployment guide](https://partner.steamgames.com/doc/steamhardware/steamframe/loadgames)
and [debugging guide](https://partner.steamgames.com/doc/steamhardware/steamframe/debugging).
