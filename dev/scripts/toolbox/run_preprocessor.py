from scripts.toolbox.helpers import log_info, log_warning, log_new_line, bash_command, bash_command_get_output, get_input
from pathlib import Path

MAKE_PREPROCESSING_PREFIX = "Preprocessing CXX source to "

def run_preprocessor_impl(build_dir, cmake_path, source_file):
    log_info("This command will not work with ninja and might not work on non-linux system!")

    cmake_dir = Path(build_dir) / cmake_path

    log_info(f"Running preprocessor for {source_file}.i in {cmake_dir}")

    out, _ = bash_command_get_output(f"make {source_file}.i", cwd = cmake_dir)


    if MAKE_PREPROCESSING_PREFIX not in out:
        log_warning("Something went wrong!")
        log_warning(f"Make output: {out}")
        return
    
    i_file = out[out.find(MAKE_PREPROCESSING_PREFIX) + len(MAKE_PREPROCESSING_PREFIX):].strip()

    log_info(f"Preprocessed file is at {i_file}")

    open_vscode = get_input("Open file in vscode? [Y/n]: ")
    if open_vscode.lower() in ["y"]:
        bash_command(f"code {cmake_dir / i_file}")
