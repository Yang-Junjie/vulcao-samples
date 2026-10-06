# vulcao-samples

Small, focused programs built with [vulcao](https://github.com/Yang-Junjie/vulcao),
a thin RAII wrapper around Vulkan. The samples live in their own repository so the
library stays dependency-light: GLFW and GLM are pulled in here, not by vulcao.

## Samples

Each directory under [`samples/`](samples) builds a standalone executable. Its
target name is the directory name without the numeric prefix, for example
`12_compute_reduction` builds `compute_reduction`.

Headless samples run without a display, verify GPU results on the CPU, print a
result and exit. Window samples render until closed. New samples use procedural
geometry and textures, so they need no additional assets.

| Sample | Mode | Focus |
| --- | --- | --- |
| [01_hello_triangle](samples/01_hello_triangle) | Window | Vertex/index buffers, reflected vertex layout, graphics pipeline and presentation |
| [02_uniforms](samples/02_uniforms) | Window | Animated transforms with a uniform buffer and descriptor set per frame |
| [03_compute](samples/03_compute) | Headless | Storage buffers, compute dispatch, barriers and verified readback |
| [04_offscreen](samples/04_offscreen) | Headless | Offscreen color attachment, push constants and pixel checks |
| [05_texture](samples/05_texture) | Window | PNG loading, mipmap generation and a combined image sampler |
| [06_two_pass](samples/06_two_pass) | Window | OBJ mesh, depth testing, lighting, HDR rendering and a fullscreen post pass |
| [07_instancing](samples/07_instancing) | Window | 96 instances in one draw, separate vertex/instance bindings and animated push constants |
| [08_depth_blending](samples/08_depth_blending) | Headless | Opaque depth rejection, transparent layers with depth writes disabled and scissor clipping |
| [09_msaa](samples/09_msaa) | Headless | Multisample color attachment, dynamic-rendering resolve and partially covered edge pixels |
| [10_multiple_render_targets](samples/10_multiple_render_targets) | Headless | Three simultaneous outputs: albedo, normal/roughness and an integer object-picking ID |
| [11_storage_image](samples/11_storage_image) | Headless | Compute-generated RGBA texture, storage-to-sampled transition and a graphics pass |
| [12_compute_reduction](samples/12_compute_reduction) | Headless | Workgroup shared memory, thread barriers, multiple passes and ping-pong buffers |
| [13_indirect_draw](samples/13_indirect_draw) | Headless | Indirect dispatch, GPU-generated geometry/draw arguments and indirect drawing |
| [14_dynamic_uniforms](samples/14_dynamic_uniforms) | Headless | Aligned object records, dynamic uniform offsets and one descriptor set reused for three draws |
| [15_texture_array](samples/15_texture_array) | Headless | Layered image uploads, a 2D-array view, separate image/sampler descriptors and viewports |
| [16_async_transfer](samples/16_async_transfer) | Headless | Asynchronous buffer/image uploads, timeline semaphore waits and optional separate transfer queue |
| [17_queries](samples/17_queries) | Headless | Visible/occluded draws, query-result copies and timestamp conversion with wrap handling |
| [18_specialization_cache](samples/18_specialization_cache) | Headless | Three specialization variants, reflected descriptor-layout caching and serialized pipeline-cache reload |
| [19_parallel_recording](samples/19_parallel_recording) | Headless | Four worker threads, one command pool per thread and secondary buffers inside dynamic rendering |
| [20_transfer_regions](samples/20_transfer_regions) | Headless | Buffer fill/update, partial image upload, row pitches, byte offsets, padding and mip-level readback |
| [21_descriptor_indexing](samples/21_descriptor_indexing) | Headless | Runtime descriptor arrays, nonuniform indexing, partial binding, variable counts and update-after-bind |
| [22_deferred_destruction](samples/22_deferred_destruction) | Window | A new vertex buffer each frame, three frames in flight and fence-based deferred resource destruction |
| [23_texel_buffer](samples/23_texel_buffer) | Headless | Typed R32Uint buffer views and uniform/storage texel-buffer descriptors |
| [24_cubemap](samples/24_cubemap) | Headless | Six-face upload, cube views, directional sampling and two reflected descriptor sets |

For compute work, start with `03`, then try `12`, `11` and `13`. For descriptor
usage, follow `02`, `14`, `15`, `24` and `21`. For resource lifetime and scheduling,
look at `16`, `19` and `22`. Vulkan operations stay in each sample's `main.cpp`;
[`common/sample.h`](samples/common/sample.h) only shares logging, shader loading
and verification helpers, and [`common/window_loop.h`](samples/common/window_loop.h)
handles frame limits, resizing and minimized windows for `07` and `22`.

## Requirements

- CMake 3.24+
- A C++20 compiler
- The Vulkan SDK (also provides `slangc`, used to compile the shaders)
- A Vulkan 1.3 device and driver

Samples `07`–`24` request validation in both Debug and Release builds. Install the
SDK's validation layer to get validation diagnostics. The original samples use
vulcao's default: validation enabled in Debug builds.

## Building

`vulcao` is a git submodule and is built from source as part of this project:

```sh
git clone https://github.com/Yang-Junjie/vulcao-samples
cd vulcao-samples
git submodule update --init third_party/glfw third_party/glm third_party/vulcao
git -C third_party/vulcao submodule update --init \
    third_party/vk-bootstrap third_party/VulkanMemoryAllocator third_party/spirv-reflect
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

A `git clone --recursive` also works, but it initializes every submodule of every
dependency, which is more than this project needs.

To build just one sample:

```sh
cmake --build build --config Debug --target compute_reduction
```

Run samples from any working directory; shader and asset paths are supplied by
CMake. For Ninja or Makefile builds:

```sh
./build/samples/01_hello_triangle/hello_triangle
./build/samples/02_uniforms/uniforms
./build/samples/03_compute/compute
./build/samples/04_offscreen/offscreen
./build/samples/05_texture/texture
./build/samples/06_two_pass/two_pass
./build/samples/07_instancing/instancing --frames 120
./build/samples/12_compute_reduction/compute_reduction
./build/samples/16_async_transfer/async_transfer --dedicated
./build/samples/22_deferred_destruction/deferred_destruction --frames 120
./build/samples/24_cubemap/cubemap
```

On Windows, executables have an `.exe` suffix. Visual Studio and other
multi-configuration generators also insert the configuration directory:

```powershell
.\build\samples\07_instancing\Debug\instancing.exe --frames 120
```

`05_texture` accepts an image path as its first argument. `07_instancing` and
`22_deferred_destruction` accept `--frames N` for a positive frame count; without
it they run interactively. Escape closes either new window sample.

## Verification

CTest registers the headless samples by default, including both queue modes of
`16_async_transfer`:

```sh
ctest --test-dir build -C Debug --output-on-failure -L headless
```

To include finite window smoke tests for `07` and `22` (requires a display):

```sh
cmake -S . -B build -DVULCAO_SAMPLES_WINDOW_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

For synchronization validation with current SDK validation layers, set
`VK_LAYER_VALIDATE_SYNC=1` before running CTest. In PowerShell:

```powershell
$env:VK_LAYER_VALIDATE_SYNC = "1"
ctest --test-dir build -C Debug --output-on-failure
```

In a POSIX shell:

```sh
VK_LAYER_VALIDATE_SYNC=1 ctest --test-dir build -C Debug --output-on-failure
```

Incorrect output, exceptions and validation errors return a failure status. New
samples capture validation errors through resource destruction, including messages
from worker threads. GPU timestamps are informational and have no performance
pass/fail threshold. The reduction checks sizes `1`, `63`, `64`, `65`, `1003` and
`4099`; image and descriptor examples also exercise incomplete workgroups, aligned
offsets and unused array slots.

Optional hardware support is handled explicitly:

- `09_msaa` chooses 4x or 2x according to supported sample counts.
- `16_async_transfer` uses the graphics queue by default. `--dedicated` requests
  a distinct transfer queue family and uses concurrent resource sharing.
- `17_queries` always checks occlusion and reports if the graphics queue has no
  timestamps.
- `21_descriptor_indexing` checks and requests only the five descriptor-indexing
  features it uses.
- `23_texel_buffer` checks uniform/storage texel-buffer support for R32Uint.

When a required optional capability is absent, the affected sample prints `SKIP`
and returns `77`, which CTest reports as skipped. Other errors remain failures.
Use `-DBUILD_TESTING=OFF` to disable test registration while keeping the sample
executables available.

## License

MIT, see [LICENSE](LICENSE).
