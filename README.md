# Vulkan Dual GPU PoC (buffer export path)

Proof of concept for a dual-GPU Vulkan pipeline on Linux.

Current architecture:

- GPU B renders into a local image
- GPU B copies image -> exportable shared buffer (OPAQUE_FD)
- GPU A imports the shared buffer
- GPU A copies buffer -> local image
- GPU A blits local image -> swapchain

This snapshot is the current exported project state from the conversation.


## Device-group pivot note

This exported snapshot includes `src/device_group.h` and `src/device_group.cpp` plus `docs/DEVICE_GROUP_PIVOT.md` as the next-step pivot away from the failing cross-device `OPAQUE_FD` import path. The current source tree remains the last compiling baseline; the device-group path is included as scaffold for the next implementation step.
