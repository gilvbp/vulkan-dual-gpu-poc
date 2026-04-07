# Device group pivot scaffold

This snapshot keeps the last compiling project state and adds the first source files for the `VK_KHR_device_group` / Vulkan 1.1+ device-group pivot:

- `src/device_group.h`
- `src/device_group.cpp`

Why this pivot exists:
- direct `OPAQUE_FD` import for shared image memory failed at runtime on the user's NVIDIA/Linux setup
- direct `OPAQUE_FD` import for shared buffer memory also failed at runtime
- the next architecture to try is a single logical device spanning both RTX 3090 GPUs via `vkEnumeratePhysicalDeviceGroups` and `VkDeviceGroupDeviceCreateInfo`

Important:
- This ZIP is a **source snapshot** prepared for the device-group pivot.
- It preserves the currently compiling project baseline.
- The device-group pivot is **not fully wired into `app.cpp`/`app.h` yet** in this exported snapshot.
- It is intended as the next implementation step, not as a runtime-validated final build.

Recommended next merge steps:
1. Replace dual-`VkDevice` creation in `App` with one `VkDevice` created from `VkDeviceGroupDeviceCreateInfo`.
2. Add group-aware submit/present structures:
   - `VkDeviceGroupSubmitInfo`
   - `VkDeviceGroupPresentInfoKHR`
3. Move per-GPU work selection to device masks/device indices.
4. Remove the current external-memory import/export path once device-group rendering path is in place.
