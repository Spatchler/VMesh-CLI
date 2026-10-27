# VMesh CLI

A CLI tool to generate voxel data from a triangle mesh using my C++ library [VMesh](https://github.com/Spatchler/VMesh). The tool uses a custom file format for data and JASC-PAL for the palette. It can also do out of core generation for larger octrees.

## Usage:

```
Usage: vmesh [OPTIONS] [SOURCE] [DEST(optional)]

Mesh voxelizer

Options:
  -h, --help                         -produce help message 
  -v, --verbose                      -verbose output 
  -f, --format arg                   -specify output format (vmu, vmc, vm8, 
                                      vm64) 
  -p, --palette arg                  -specify path to an existing palette 
                                      to use rather than create one 
  -r, --resolution arg (=128)        -set voxel grid resolution 
  -l, --subdivision-level arg (=0)   -set depth to generate initial subtrees 
                                      before combining for out of core generation 
  -b, --binary                       -generate binary voxel data instead of 
                                      coloured voxel data 
  --scale-mode arg (=proportional)   -scaling mode (proportional, stretch, 
                                      none) 
  --colour-distance arg (=0.1)       -set the minimum euclidean distance between 
                                      two normalized rgb colours that is required 
                                      for a new colour to be added to the 
                                      palette 
  -m, --metadata                     -print file metadata 
  --tribox                           -use triangle box intersections instead 
                                      of DDA voxelization, only supports binary 
                                      data and slower 
  --matrixBarycentric                -use inverse matrix method for barycentric 
                                      coordinate calculation instead of the 
                                      default which uses Cramer's rule 
```

## Dependencies:

- VMesh
- assimp
- Boost

Please create an issue if you have trouble with anything.

## Build:

`premake5 gmake && make config=dist`
