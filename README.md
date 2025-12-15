# VMesh CLI

A CLI tool to generate voxel data from a triangle mesh using my C++ library [`VMesh`](https://github.com/Spatchler/VMesh). The tool simply outputs a binary file.

### Usage:
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

### Dependencies:
* VMesh
* ASSIMP
* Boost Program Options

### Build:
`premake5 gmake && make`

