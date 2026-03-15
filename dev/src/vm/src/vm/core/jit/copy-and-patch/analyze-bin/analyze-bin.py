# Gathers data from the binary stencils used for embeding and patching

import _llvm
import asyncio
import click

async def generate_stencils(binary: str, output_file):
    nm_args = ["-S", "--demangle", "--format=posix", binary]
    nm_output: str = await _llvm.run("llvm-nm", nm_args)

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
    '--output', 
    required=True, 
    type=click.File('w'), 
    help='File where stencils will be written.'
)
@click.argument(
    'binary', 
    type=click.Path(exists=True)
)
def main(output, binary):
    asyncio.run(generate_stencils(binary, output))

if __name__ == "__main__":
    main()