# Dragonfly - Volumetric Video Player

Dragonfly is a volumetric video system that captures 3D scenes, encodes them as voxel streams, and plays them back in VR with full 6 degrees of freedom. Unlike 360 stereo videos which offer only fixed-viewpoint playback, Dragonfly lets the viewer freely move and rotate their head within a 1 m&#x00B3; headbox, providing true depth and parallax.

The Dragonfly player was released on the Oculus Store in 2016, showcasing volumetric videos captured from Unreal Engine 4.

**Demo:** https://www.youtube.com/watch?v=qJhnPsNcy2g

## How It Works

The system operates in two phases:

### Offline: Capture and Encode

1. **Capture** - An Unreal Engine 4 plugin renders color and depth buffers from 102 viewpoints per frame (17 cubemaps arranged in a 1 m&#x00B3; volume). A GPU compute pass unprojects these pixels into 3D points and inserts them into an octree of 1200&#x00B3; resolution. The octree is dumped as blocks of 4x4x4 voxels. Full capture takes ~5 seconds per frame.

2. **Encode** - Voxel blocks are compressed in three stages:
   - **Block compression**: each 4x4x4 block (64 voxels) is reduced from 2048 bytes to 228 bytes using a BC1-like color encoding with a visibility bitmask.
   - **Delta compression**: blocks that persist across consecutive frames are merged. Frames use I-frame / P-frame structure (1 I-frame per second + N P-frames).
   - **Stream compression**: data is laid out as Structure of Arrays (positions, visibilities, colors separated) and compressed with Zstandard for ~4:1 lossless ratio. Full encoding takes ~5 seconds per frame.

   The resulting `.df` stream files have a typical bitrate of 20-40 MB/s at 45 FPS capture rate.

### Real-Time: Player

The player streams and decompresses voxel data on the CPU, then renders on the GPU via a compute shader pipeline:

1. **Cull** (~80 us) - Frustum-cull voxel blocks against the current viewpoint
2. **Project** (~240 us) - Assign visible blocks to screen-space 8x8 pixel tiles
3. **Trace** (~2800 us) - For each pixel, ray-cast against all blocks overlapping its tile to find the closest voxel hit
4. **TSAA** (~360 us) - Temporal Supersampling Anti-Aliasing to smooth voxel edges

Benchmarked at 90 Hz stereo rendering (2800x1400) with ~10% CPU load (4 cores @ 4 GHz) and ~30% GPU load (GeForce GTX 1070).

## Technical Details

- **Platform**: Windows x64, Direct3D 11, HLSL Shader Model 5.0
- **VR**: Oculus Rift (DK2 and CV1) via LibOVR SDK
- **Capture source**: Any renderer exposing color + depth buffers (UE4 plugin provided)
- **Voxel grid**: up to 1200&#x00B3; resolution per frame
- **Compression**: LZ4 and Zstandard
- **Player binary**: self-contained, ~750 KB

### Known Limitations

- Only computer-generated (CG) graphics are supported
- Screen-space effects are not captured (not well suited for 3D capture)
- Transparency is not supported
- Camera-dependent geometry (billboards) should be avoided
- Rendering quality degrades outside the headbox boundaries

## Project Structure

```
source/v6/
  core/       Platform layer (memory, threading, file I/O, math, streams)
  codec/      V6 codec (encoder, decoder, compression)
  graphic/    D3D11 GPU abstraction, voxel ray-tracing pipeline, font, HMD
  player/     VR player application (Oculus Store release)
  viewer/     Engineering viewer/encoder tool
  encoder/    CLI stream encoder
  compressor/ Voxel block compression benchmarks
  tracer/     Ray-tracing analysis tool
```

## Documentation

- [Technical Overview (PDF)](doc/volumetric_video_project_2016.pdf)

## Author

Gilles Cadet - [SupraWings](http://www.video6dof.com/)

## License

[MIT](LICENSE)
