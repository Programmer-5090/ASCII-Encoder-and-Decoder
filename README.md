# ASCII Encoder and Decoder

Converts images and videos into coloured ASCII art, then renders and saves them as PNG, JPG, MP4, or GIF.

## Features

- Convert a still image to a colour ASCII art PNG/JPG
- Convert a video to a colour ASCII art MP4 or GIF
- Custom Huffman + delta-encoding codec to compress ASCII video frames to a binary file and decompress them back

## Demo

A demo video showing the output of the converter can be found here:

**Demo Video**

<video src='readme\demo.mp4' width=720/>;


### File Size Warning

Output files can become **very large**, especially for GIF and MP4:

- **GIF files** are particularly large (often 50 to 500 MB+ depending on length and resolution)
- **MP4 files** are more compressed (typically 10 to 250 MB+ for videos of 30s)
- **Compressed binary** (`.bin`) files from the codec are much smaller but are intermediate representations

Plan your disk space accordingly, especially for long videos or high-resolution output.

## Dependencies

All dependencies are managed via [vcpkg](https://github.com/microsoft/vcpkg) and declared in `vcpkg.json`:

| Library | Purpose |
|---|---|
| SDL3 | Surface creation and blitting |
| SDL3_ttf | Font rasterisation |
| OpenCV | Image/video loading and MP4 writing |

## Building

### Prerequisites

- CMake = 3.20
- A C++23 compiler (MSVC 2022 recommended on Windows)
- [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set in your environment

### Steps

Configure and build using CMake's default generator (Visual Studio on Windows, Make on Linux/macOS):

```bash
cmake -S . -B build
cmake --build build --config Release
```

If you prefer [Ninja](https://ninja-build.org/) (faster incremental builds, requires Ninja on `PATH`):

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

On Windows you can also use the provided helper script, which uses the default generator:

```bat
build.bat
```

vcpkg will automatically install all dependencies on the first configure via manifest mode.

## Usage

Place your assets in the `assets/` folder:

```
assets/
  your_video.mp4
  your_image.jpg
  Boogaloo-Regular.ttf   # or any monospace-friendly font
```

Edit the asset paths at the top of `main()` in `src/main.cpp`, then run the built executable. Output files are written to the `out/` directory.

## Project Structure

```
src/
  main.cpp          # Entry point and rendering pipeline
  codec.h           # Huffman + delta encoding/decoding for ASCII video
  gif.h             # GIF writer (single-header)
  stb_image_write.h # PNG/JPG writer (single-header, stb)
assets/             # Input media and fonts (not tracked by git)
readme/             # Demo video
CMakeLists.txt
vcpkg.json
