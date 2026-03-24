# Vulkan Dual GPU PoC (Linux / NVLink / External Memory)

Este projeto é um **scaffold completo** para a PoC de dual-GPU em Vulkan no Linux:

- **GPU B** renderiza para uma `VkImage` exportável
- a imagem é compartilhada por **external memory FD**
- **GPU A** importa a imagem
- **timeline semaphore / external semaphore FD** fazem a sincronização
- **GPU A** compõe e apresenta no swapchain

## Status

O projeto está organizado com todos os módulos principais da PoC, mas ainda é um **scaffold técnico**:

- a maior parte do pipeline e da arquitetura já está distribuída em arquivos reais
- vários pontos estão marcados com `TODO` para a integração final de surface/swapchain/shader modules/descriptor sets
- serve como base séria para evoluir a prova de conceito

## Estrutura

- `src/app.*` – coordenação geral
- `src/vk_instance.*` – criação de instance
- `src/gpu_select.*` – enumeração de GPUs
- `src/shared_image.*` – export/import de imagens por FD
- `src/timeline_sync.*` – timeline semaphores e FD import/export
- `src/device_b_render.*` – lado da GPU B
- `src/device_a_present.*` – lado da GPU A
- `shaders/` – shaders base

## Build

```bash
mkdir build
cd build
cmake ..
make -j
```

## Observações

- A criação de `VkSurfaceKHR` e do swapchain real depende da camada de janela que você quiser usar (GLFW, SDL, Xlib, Wayland direto).
- A bridge NVLink/PCIe continua sendo decidida pelo driver, mas a arquitetura desta PoC foi pensada para medir esse custo no nível do app.
