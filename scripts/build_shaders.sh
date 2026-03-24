#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."
glslc shaders/fullscreen.vert -o shaders/fullscreen.vert.spv
glslc shaders/blit.frag -o shaders/blit.frag.spv
glslc shaders/passthrough.comp -o shaders/passthrough.comp.spv
