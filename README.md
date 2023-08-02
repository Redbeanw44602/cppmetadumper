# VTable Exporter
It can extract virtual table resources, such as RTTI or virtual function table, from ELF files.

## Usage
```
Usage: VTable Exporter [-h] --output VAR [--export-rtti] [--export-vtable] [--dump-segment] [--no-rva] target

Positional arguments:
  target                Path to a valid ELF file. [required]

Optional arguments:
  -h, --help            shows help message and exits
  -v, --version         prints version information and exits
  -o, --output          Path to save the result, in JSON format. [required]
  --export-rtti         Export all RTTI information from ELF file.
  --export-vtable       Export all virtual function information from the ELF file.
  --dump-segment        Save the .data.rel.ro segment as segment.dump.
  --no-rva              I don't care about the RVA data for that image.
```
If I now need to extract RTTI information from `libsample.so`, I can use the following command (assuming `libsample.so` is in the current directory).  
```batch
VTable-Exporter "libsample.so" --export-rtti -o "sample.json"
```
The resulting will be saved in JSON format.  

## Features
 - Automatically rebuild .data.rel.ro (if needed).
 - Export RTTI perfectly.

## Limitation
 - Supported platforms: `AArch64`, `X86_64`.
 - Exporting a virtual function table requires a symbol table (`.symtab`).
 - If you need to export RTTI, make sure the target binary was compiled without `-fno-rtti`.
 - (bug) **If your file requires to rebuilt data.rel.ro, then the RVA of the external symbol may be wrong.** This problem affects the export results of VTable, but not RTTI.
 - (bug) Alignment in vtables cannot be resolved for now. Therefore, the export of some vtables will prompt failure, but RTTI will not be affected.

## Have a problem?
Please send an Issue with the binary file.  
Let's learn together :)

## License
MIT
