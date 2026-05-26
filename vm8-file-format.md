### Description:

File format for storing index based sparse voxel octrees. Often paired with a JASC-PAL file.

UINT_MAX - palette size = air index

An index greater than air index is used to index the palette otherwise it indexes another node in the indices array.

### Contents:

| Bytes   | Type       | Value                                                  |
| :------ | :--------- | :----------------------------------------------------- |
| 1\*6    | char       | id 'VMESH8' : 'V' 'M' 'E' 'S' 'H' '8', 'V' is first    |
| 4       | uint       | version number : 100                                   |
| 4       | uint       | grid resolution                                        |
| 4       | uint       | palette size not including air                         |
| 4       | uint       | indices count (N)                                      |
| 4\*8\*N | uint       | indices                                                |

### Loading example:

```cpp
void load(const std::filesyste::path& path, uint32_t& resolution, uint32_t& paletteSize, std::vector<std::array<uint32_t, 8>> indices) {
  // Open file
  std::ifstream fin;
  fin.open(path, std::ios::binary | std::ios::in);
  if (!fin.is_open()) throw std::runtime_error("Could not open file");
  
  // Check header
  std::string line;
  std::getline(fin, line);
  if (line != "VMeshOctree") throw std::invalid_argument("Octree file incorrect format");
  std::getline(fin, line);
  if (line != "0100") throw std::invalid_argument("Octree file incorrect format version");

  // Check header
  std::string str;
  str.resize(6);
  fin.read(str.data(), 6);
  if (str != "VMESH8") throw std::invalid_argument("Invalid octree file");
  uint32_t version;
  fin.read(reinterpret_cast<char*>(&version), sizeof(version));
  if (ver != 100) throw std::invalid_argument("Invalid octree file version");

  // Read resolution
  fin.read(reinterpret_cast<char*>(&resolution), 4);
  
  // Read palette size
  fin.read(reinterpret_cast<char*>(&paletteSize), 4);
  
  // Read indices
  uint32_t numIndices;
  fin.read(reinterpret_cast<char*>(&numIndices), 4);
  indices.resize(numIndices);
  fin.read(reinterpret_cast<char*>(indices.data()), sizeof(indices.at(0)) * indices.size());
  
  // Close
  fin.close();
}
```

