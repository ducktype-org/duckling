#!/usr/bin/env python3
import os
import shutil
import subprocess
import argparse

def compile_and_copy_binary(build_dir_name="build"):
    # Define paths (build dir name is used relative to two levels up)
    build_dir = os.path.abspath(os.path.join("..", "..", build_dir_name))
    binary_name = "lsp_daemon"
    source_binary_path = os.path.join(build_dir, "bin", binary_name)
    destination_dir = os.path.abspath("./bin")
    destination_binary_path = os.path.join(destination_dir, binary_name)

    # Step 1: Go to build_dir and execute "ninja lsp_daemon"
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

    # Step 3: Copy the binary file from build_dir/bin to ./bin
    print(f"Copying binary from {source_binary_path} to {destination_binary_path}...")
    os.makedirs(destination_dir, exist_ok=True)
    shutil.copy2(source_binary_path, destination_binary_path)

    print("Binary successfully compiled and copied.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compile and copy lsp_daemon from build directory.")
    parser.add_argument("build_dir_name", nargs="?", default="build",
                        help='Name of the build directory (default: "build")')
    args = parser.parse_args()
    compile_and_copy_binary(args.build_dir_name)
