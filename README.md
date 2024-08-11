# cppmetadumper
This is a tool that can extract "C++ meta information" from executable files, such as `Vftable` and `RTTI`.

## Usage
```
Usage: cppmetadumper [-h] --output VAR target

Positional arguments:
  target        Path to a valid executable. [required]

Optional arguments:
  -h, --help    shows help message and exits 
  -v, --version prints version information and exits 
  -o, --output  Path to save the result, in JSON format. [required]
```
If I now need to extract RTTI information from `libsample.so`:
```bash
./cppmetadumper "libsample.so" -o "sample.json"
```
The resulting will be saved in JSON format.  

## Features
 - Supported platforms: `aarch64`, `x86_64`.
 - Supported formats: `ELF64`.
 - Automatically rebuild `.data.rel.ro`.
 - Export RTTI perfectly.

## TODOs
 - [ ] Mach-O support.
 - [ ] PE support.
 - [ ] Virtual inheritance support.

## For GCC/Clang compilation results
 - At least one of the symbol table or RTTI is required to properly identify and export the vftable.
 - If a symbol table is not provided, some unusual styles of vftables will not be recognized.

## Known issues
 - The vftable export result is not guaranteed to be completely correct.
 - If your file requires to rebuilt data.rel.ro, then the RVA of the external symbol may be wrong. This problem affects the export results of Vftable, but not RTTI.
> If you know how to solve it, please let me know ;)

## Have a problem?
Please send an Issue with the binary file.  
Let's learn together :)

## License
MIT
