# vulcao-samples

Small, focused programs built with [vulcao](https://github.com/Yang-Junjie/vulcao),
a thin RAII wrapper around Vulkan. The samples live in their own repository so the
library stays dependency-light: GLFW and GLM are pulled in here, not by vulcao.

## Samples

The programs live under [`samples/`](samples):

- `01_hello_triangle`: a CPU-side vertex and index buffer drawn through a full
  graphics pipeline.
- `02_uniforms`: the triangle spun by a uniform buffer and descriptor set.
- `03_compute`: a storage buffer transformed by a compute shader, with the result
  verified on the CPU. Needs no display, and exits non-zero on a wrong result or
  on a validation error.
- `04_offscreen`: a triangle rendered into an image, then read back and checked
  pixel by pixel. Also needs no display.
- `05_texture`: a PNG decoded at run time, uploaded as a mipmapped texture and
  sampled through a combined image sampler onto a quad that fills the window.
  Opens a window; pass any image path as the first argument to show another one.
- `06_two_pass`: an offscreen scene pass composited by a second full-screen pass.

## Requirements

- CMake 3.24+
- A C++20 compiler
- The Vulkan SDK (also provides `slangc`, used to compile the shaders)

## Building

`vulcao` is a git submodule and is built from source as part of this project:

```sh
git clone https://github.com/Yang-Junjie/vulcao-samples
cd vulcao-samples
git submodule update --init third_party/glfw third_party/glm third_party/vulcao
git -C third_party/vulcao submodule update --init \
    third_party/vk-bootstrap third_party/VulkanMemoryAllocator third_party/spirv-reflect
cmake -S . -B build
cmake --build build
```

A `git clone --recursive` also works, but it initializes every submodule of every
dependency, which is more than this project needs.

Run a sample, for example:

```sh
./build/samples/01_hello_triangle/hello_triangle
./build/samples/02_uniforms/uniforms
./build/samples/03_compute/compute
./build/samples/04_offscreen/offscreen
./build/samples/05_texture/texture
./build/samples/06_two_pass/two_pass
```

## License

MIT, see [LICENSE](LICENSE).
