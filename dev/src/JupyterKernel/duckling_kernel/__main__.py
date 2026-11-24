# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""`python -m duckling_kernel install` installs the kernel spec; otherwise launches the kernel."""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path


def install(argv: list[str]) -> None:
    from jupyter_client.kernelspec import KernelSpecManager

    parser = argparse.ArgumentParser(prog="python -m duckling_kernel install")
    parser.add_argument("--name", default="duckling", help="kernel name")
    where = parser.add_mutually_exclusive_group()
    where.add_argument("--user", action="store_true", help="install for the current user")
    where.add_argument("--sys-prefix", action="store_true", help="install into sys.prefix")
    where.add_argument("--prefix", help="install under this prefix")
    args = parser.parse_args(argv)

    spec = {
        "argv": [sys.executable, "-m", "duckling_kernel", "-f", "{connection_file}"],
        "display_name": "Duckling",
        "language": "duckling",
    }
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / "kernel.json").write_text(json.dumps(spec, indent=2))
        prefix = sys.prefix if args.sys_prefix else args.prefix
        dest = KernelSpecManager().install_kernel_spec(
            tmp, args.name, user=args.user, prefix=prefix
        )
    print(f"Installed the Duckling kernel spec in {dest}")


def main() -> None:
    if len(sys.argv) > 1 and sys.argv[1] == "install":
        install(sys.argv[2:])
        return
    from ipykernel.kernelapp import IPKernelApp

    from .kernel import DucklingKernel

    IPKernelApp.launch_instance(kernel_class=DucklingKernel)


if __name__ == "__main__":
    main()
