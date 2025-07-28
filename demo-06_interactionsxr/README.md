# demo-06_interactionsxr

[![Demo video](https://img.youtube.com/vi/gKtBfLZGCbk/hqdefault.jpg)](https://youtu.be/gKtBfLZGCbk)

## EXT Hand Interaction extension demo using [xrlib](https://github.com/1runeberg/xrlib), optionally the following:
 - Mesh projection passthrough (via fb triangle mesh)
 - Simultaneous hands and controllers (meta)
 - Use new System Properties helper to probe openxr runtime system/hardware capabilities

## Mechanics:
- With handtracking, create a pinch gesture on one hand (tip of pointer finger and thumb together), the value of the "pinch" action affects the scale of the pinch indicator cube which uses the current frame pose of the pinch action.
- With controller, indicators appear for the controller grip pose. Simultaneous hand tracking and controller is supported on both SteamVR and Meta devices.
- With handtracking, grasp gesture (fist) increases the size of hte passthrough window (white plane if fb passthrough and fb triangle mesh extensions arent available on the current openxr runtime and device)

For build instructions, see the [xrlib demos build guide](https://github.com/1runeberg/xrlib-demos)
