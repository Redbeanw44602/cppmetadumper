# VTable Exporter
This is a tool that can extract "C++ meta information" from executable files, such as `Vftable` and `RTTI`.

## Usage
```
Usage: VTable Exporter [-h] --output VAR target

Positional arguments:
  target        Path to a valid executable. [required]

Optional arguments:
  -h, --help    shows help message and exits 
  -v, --version prints version information and exits 
  -o, --output  Path to save the result, in JSON format. [required]
```
If I now need to extract RTTI information from `libsample.so`:
```bash
./VTable-Exporter "libsample.so" -o "sample.json"
```
The resulting will be saved in JSON format.  

## Features
 - Supported platforms: `AArch64`, `X86_64`.
 - Supported formats: `ELF`.
 - Automatically rebuild `.data.rel.ro`.
 - Export RTTI perfectly.

## TODOs
 - [ ] Mach-O support.
 - [ ] PE support.
 - [ ] Parsing vbtable.

## Limitation
 - Exporting a virtual function table requires a symbol table (`.symtab`).
 - If you need to export RTTI, make sure the target binary was compiled without `-fno-rtti`.
 - (bug) **If your file requires to rebuilt data.rel.ro, then the RVA of the external symbol may be wrong.** This problem affects the export results of Vftable, but not RTTI.
> If you know how to solve it, please let me know ;)

## Have a problem?
Please send an Issue with the binary file.  
Let's learn together :)

## License
MIT
