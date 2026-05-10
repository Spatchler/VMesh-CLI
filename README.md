# VMesh CLI

A CLI tool to generate voxel data from a triangle mesh using my C++ library [`VMesh`](https://github.com/Spatchler/VMesh). The tool uses a custom file format for data and JASC-PAL for the palette. It can also do out of core generation for larger octrees.

## Usage:
```
Usage: vmesh OPTIONS input-path

Mesh voxelizer

Options:
  -h [ --help ]                       produce help message
  -v [ --verbose ]                    verbose output
  -C [ --compressed ]                 compress voxel data
  -S [ --svdag ]                      generate a Sparse Voxel Octree instead of a normal voxel grid
  -R [ --resolution ] arg (=128)      set voxel grid resolution
  -L [ --subdivision-level ] arg (=0) set depth to generate initial subtrees before combining
  --scale-mode arg (=proportional)    scaling mode either (proportional, stretch, none)
  --voxel-to-svdag                    input voxel binary file and output svdag
  --tribox                            use triangle box intersections instead of DDA voxelization, 
                                      it tends to be faster on low resolutions(<512) however it 
                                      only generates binary data
  -B [ --binary ]                     generate binary voxel data instead of coloured voxel data
  --colourThreshold arg (=0.01)       set the normalized percentage colour difference threshold 
                                      that is required for a new colour to be added to the palette
```

## Loading files
### vm8 files (Sparse Voxel Octree):
vm8(VMeshOctree) is a format for storing index based octrees. It contains resolution, palette size, number of indices and the the indices themselves.

Palette size does not include air.

UINT_MAX - palette size = air index

Anything greater than air index is an index for the palette. Anything below is a child index.

Example loader:
```cpp
// Output variables
uint32_t resolution, paletteSize, numIndices;
std::vector<std::array<uint32_t, 8>> indices;

// Open file
std::ifstream fin;
fin.open("file.vm8", std::ios::binary | std::ios::in);

// Read header
{
  std::string line;
  std::getline(fin, line);
  if (line != "VMeshOctree") throw std::invalid_argument("Octree file incorrect format");
  std::getline(fin, line);
  if (line != "0100") throw std::invalid_argument("Octree file incorrect format version");
}

// Read resolution
fin.read(reinterpret_cast<char*>(&resolution), 4);

// Read palette size
fin.read(reinterpret_cast<char*>(&paletteSize), 4);

// Read num indices
fin.read(reinterpret_cast<char*>(&numIndices), 4);

// Load indices
indices.resize(numIndices);
fin.read(reinterpret_cast<char*>(indices.data()), sizeof(indices.at(0)) * indices.size());

fin.close();
```

## Dependencies:
* VMesh
* ASSIMP
* Boost Program Options

## Build:
`premake5 gmake && make config=dist`

