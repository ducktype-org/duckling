#!/usr/bin/env python3
# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import os
import shutil
import subprocess
import argparse

def compile_and_copy_binary(build_dir_name="build"):
    # Define paths (build dir name is used relative to two levels up)
    build_dir = os.path.abspath(os.path.join("..", "..", build_dir_name))
    binary_name = "duck_ls"
    source_binary_path = os.path.join(build_dir, "bin", binary_name)
    # Install to the XDG user binary directory — no sudo required, on PATH in modern distros
    destination_dir = os.path.expanduser("~/.local/bin")
    destination_binary_path = os.path.join(destination_dir, binary_name)

    # Step 1: Go to build_dir and execute "ninja duck_ls"
    print(f"Compiling the binary in {build_dir}...")
    try:
        subprocess.run(["cmake", "--build", build_dir,  "--target", binary_name], check=True)
    except subprocess.CalledProcessError as e:
        print(f"Compilation failed: {e}")
        return

    # Step 2: Check if there is a file in ./bin and delete it if it exists
    if os.path.exists(destination_binary_path):
        print(f"Removing existing binary: {destination_binary_path}")
        os.remove(destination_binary_path)

    # Step 3: Copy the binary file from build_dir/bin to ~/.local/bin
    print(f"Copying binary from {source_binary_path} to {destination_binary_path}...")
    os.makedirs(destination_dir, exist_ok=True)
    shutil.copy2(source_binary_path, destination_binary_path)

    print(f"Binary successfully installed to {destination_binary_path}.")
    print("Make sure ~/.local/bin is on your PATH. If not, add this to your shell profile:")
    print('  export PATH="$HOME/.local/bin:$PATH"')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compile and copy duck_ls from build directory.")
    parser.add_argument("build_dir_name", nargs="?", default="build",
                        help='Name of the build directory (default: "build")')
    args = parser.parse_args()
    compile_and_copy_binary(args.build_dir_name)
