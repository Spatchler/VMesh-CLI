# VMesh CLI

A CLI tool to generate voxel data from a triangle mesh using my C++ library [`VMesh`](https://github.com/Spatchler/VMesh). The tool simply outputs a binary file.

## Usage:
```
Usage: vmesh OPTIONS input-path output-path

Options:
  -h [ --help ]                  produce help message
  -v [ --verbose ]               verbose output
  -C [ --compressed ]            compress voxel data
  -S [ --svdag ]                 generate a Sparse Voxel DAG instead of a normal voxel grid
  -R [ --resolution ] arg (=100) set voxel grid resolution
  --in arg                       input file path
  --out arg                      output file path
```

## Loading files
### SVDAGs:
An SVDAG file is an array of uint32s. The first uint32 is the resolution and then from there are the indices. A value of 0 means air and a value of 1 means a solid voxel. Every value above 1 is an index for the array which you have to subtract 2 from.

Example:
```cpp
std::ifstream fin;
fin.open(path, std::ios::binary | std::ios::in);

// Read resolution
uint resolution;
fin.read(reinterpret_cast<char*>(&resolution), 4);

// Load indices
std::vector<std::array<uint32_t, 8>> indices;
uint32_t value;
uint i = 0;
std::array<uint32_t, 8> node;
while (fin.read(reinterpret_cast<char*>(&value), sizeof(uint32_t))) {
  node[i] = value;
  ++i;
  if (i == 8) {
    indices.push_back(node);
    i = 0;
  }
}
```

## Dependencies:
* VMesh
* ASSIMP
* Boost Program Options

## Build:
`premake5 gmake && make`

