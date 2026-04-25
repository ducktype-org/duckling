# Gathers data from the binary stencils used for embeding and patching

import click
import subprocess
import sys
from pathlib import Path

from llvm_tools import llvm_version_options

def run_llvm(tool: str, args) -> str:
    result = subprocess.run([tool] + args, 
        check=True, capture_output=True, text=True)
    return result.stdout

def generate_stencils(llvm_nm: str, binary: str, output_file):
    nm_args = ["-S", "--demangle", "--format=posix", binary]
    nm_output: str = run_llvm(llvm_nm, nm_args)

    for line in nm_output.split('\n'):
        if not line.strip():
            continue

        values = line.split(' ')

        *name_split, type, place_hex, size_hex = values
        place = int(place_hex, 16)
        size = int(size_hex, 16)

        name = ' '.join(name_split)
        
        output_file.write("StencilData {" + f'.name = "{name}", .type = "{type}", .place = {place}, .size = {size}' + "},\n")

@click.command()
@click.option(
    '--output', 
    required=True, 
    type=click.File('w'), 
    help='File where stencils will be written.'
)
@click.argument(
    'binary', 
    type=click.Path(exists=True)
)
@llvm_version_options
def main(llvm_nm, output, binary, **kwargs):
    generate_stencils(llvm_nm, binary, output)

if __name__ == "__main__":
    main()
