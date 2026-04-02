# Vulkan Dual GPU PoC (buffer export path)

Proof of concept for a dual-GPU Vulkan pipeline on Linux.

Current architecture:

- GPU B renders into a local image
- GPU B copies image -> exportable shared buffer (OPAQUE_FD)
- GPU A imports the shared buffer
- GPU A copies buffer -> local image
- GPU A blits local image -> swapchain

This snapshot is the current exported project state from the conversation.
