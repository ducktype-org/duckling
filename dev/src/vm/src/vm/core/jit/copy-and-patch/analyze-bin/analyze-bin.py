# Gathers data from the binary stencils used for embeding and patching

import _llvm
import logging
import asyncio
import click

async def generate_stencils(llvm_nm: str, binary: str, output_file):
    nm_args = ["-S", "--demangle", "--format=posix", binary]
    nm_output: str = await _llvm.run(llvm_nm, nm_args)

    for line in nm_output.split('\n'):
        if not line.strip():
            continue

        values = line.split(' ')

        *name_split, type, place_hex, size_hex = values
        place = int(place_hex, 16)
        size = int(size_hex, 16)

        name = ' '.join(name_split)
        
        output_file.write("{" + f'.name = "{name}", .type = "{type}", .place = {place}, .size = {size}' + "},\n")

@click.command()
@click.option(
    '--llvm-nm', 
    default="llvm-nm", 
    help='Path to LLVM-nm executable.'
)
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
def main(llvm_nm, output, binary):
    """Gathers data from binary stencils used for embedding and patching."""
    asyncio.run(generate_stencils(llvm_nm, binary, output))

if __name__ == "__main__":
    main()