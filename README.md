# VMesh CLI

A CLI tool to generate voxel data from a triangle mesh using my C++ library [VMesh](https://github.com/Spatchler/VMesh). The tool uses a custom file format for data and JASC-PAL for the palette. It can also do out of core generation for larger octrees.

## Usage:

```
Usage: vmesh OPTIONS input-path output-path(optional)

Mesh voxelizer

Options:
  -h [ --help ]                       produce help message
  -v [ --verbose ]                    verbose output
  -f [ --format ] arg                 specify output format (vmu, vmc, vm8, vm64)
  -P [ --palette ] arg                specify path to an existing palette to use rather than create
                                      one
  -R [ --resolution ] arg (=128)      set voxel grid resolution
  -L [ --subdivision-level ] arg (=0) set depth to generate initial subtrees before combining for 
                                      out of core generation
  --scale-mode arg (=proportional)    scaling mode either (proportional, stretch, none)
  --tribox                            use triangle box intersections instead of DDA voxelization, 
                                      it tends to be faster on low resolutions(<512) however it 
                                      only generates binary data
  -B [ --binary ]                     generate binary voxel data instead of coloured voxel data
  --colour-distance arg (=0.1)        set the euclidean distance between two normalized rgb colours
                                      that is required for a new colour to be added to the palette
```

## Dependencies:

* VMesh
* ASSIMP
* Boost Program Options

## Build:

`premake5 gmake && make config=dist`

