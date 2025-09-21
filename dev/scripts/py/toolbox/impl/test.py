from .helpers import (
    bash_command,
)


def test_impl(build_dir, memcheck):
    if memcheck:
        bash_command(f"cmake --build {build_dir} -- memcheck_test")
    else:
        bash_command(f"cmake --build {build_dir} -- test")
