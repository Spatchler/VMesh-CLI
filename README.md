# VMesh CLI

A CLI tool to generate voxel data from a triangle mesh using my C++ library [`VMesh`](https://github.com/Spatchler/VMesh). The tool simply outputs a binary file.

## Usage:
```
Usage: vmesh OPTIONS input-path output-path

Options:
  -h [ --help ]                       produce help message
  -v [ --verbose ]                    verbose output
  -C [ --compressed ]                 compress voxel data
  -S [ --svdag ]                      generate a Sparse Voxel Octree instead of a normal voxel grid
  -R [ --resolution ] arg (=128)      set voxel grid resolution
  -L [ --subdivision-level ] arg (=0) set depth to generate initial subtrees before combining
  --scale-mode arg (=proportional)    scaling mode either (proportional, stretch, none)
  --voxel-to-svdag                    input voxel binary file and output svdag
  --DDA                               use DDA voxelization algorithm instead of triangle box 
                                      intersections, it tends to be faster on higher resolutions
```

## Loading files
### SVDAGs:
An SVDAG file is an array of uint32s. In the order: resolution, palette size, indices size, indices.

Palette size does not include air.

UINT_MAX - palette size = air index

Anything greater than air index is an index for the palette. Anything below is a child index.

Example:
```cpp
// Output variables
uint32_t resolution, paletteSize, numIndices;
std::vector<std::array<uint32_t, 8>> indices;

// Open file
std::ifstream fin;
fin.open("file.bin", std::ios::binary | std::ios::in);

// Read resolution
fin.read(reinterpret_cast<char*>(&resolution), 4);

// Read palette size
fin.read(reinterpret_cast<char*>(&paletteSize), 4);

// Read num indices
fin.read(reinterpret_cast<char*>(&numIndices), 4);

// Load indices
indices.resize(numIndices);
fin.read(reinterpret_cast<char*>(&indices[0]), sizeof(indices[0]) * indices.size());

fin.close();
```

## Dependencies:
* VMesh
* ASSIMP
* Boost Program Options

## Build:
`premake5 gmake && make`

