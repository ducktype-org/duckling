from .helpers import (
    bash_command,
)


def docs_impl(build_dir):
    bash_command(f"cmake --build {build_dir} -- docs")
    bash_command(f"cmake --build {build_dir} -- open-sphinx-docs")
