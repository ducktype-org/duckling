# Gathers data from the binary stencils used for embeding and patching

import _llvm
import argparse
import logging
import asyncio

async def generate_stencils(llvm_nm: str, binary: str):
    nm_args = ["-S", "--demangle", "--format=posix", binary]
    nm_output: str = await _llvm.run(llvm_nm, nm_args)

    for line in nm_output.split('\n'):
        if line == "":
            continue

        values = line.split(' ')
        if len(values) != 4:
            logging.error(f"Unexpected: '{values}'")
            return

        name, type, place_hex, size_hex = values
        place = int(place_hex, 16)
        size = int(size_hex, 16)
        print("{" + f'.name = "{name}", .type = "{type}", .place = {place}, .size = {size}' + "},")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-nm", help='path to LLVM-nm', default="llvm-nm")
    parser.add_argument("binary", help='path to the binary')
    arguments = parser.parse_args()

    asyncio.run(generate_stencils(arguments.llvm_nm, arguments.binary))

if __name__ == "__main__":
    main()
