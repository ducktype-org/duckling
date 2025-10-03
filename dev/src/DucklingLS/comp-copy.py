import os
import shutil
import subprocess

def compile_and_copy_binary():
    # Define paths
    build_dir = os.path.abspath("../../build")
    binary_name = "lsp_daemon"
    source_binary_path = os.path.join(build_dir, "bin", binary_name)
    destination_dir = os.path.abspath("./bin")
    destination_binary_path = os.path.join(destination_dir, binary_name)

    # Step 1: Go to ../../build and execute "ninja lsp_daemon"
    print("Compiling the binary...")
    try:
        subprocess.run(["ninja", binary_name], cwd=build_dir, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Compilation failed: {e}")
        return

    # Step 2: Check if there is a file in ./bin and delete it if it exists
    if os.path.exists(destination_binary_path):
        print(f"Removing existing binary: {destination_binary_path}")
        os.remove(destination_binary_path)

    # Step 3: Copy the binary file from ../../build/bin to ./bin
    print(f"Copying binary from {source_binary_path} to {destination_binary_path}...")
    os.makedirs(destination_dir, exist_ok=True)
    shutil.copy2(source_binary_path, destination_binary_path)

    print("Binary successfully compiled and copied.")

if __name__ == "__main__":
    compile_and_copy_binary()