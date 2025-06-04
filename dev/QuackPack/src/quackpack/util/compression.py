import tarfile
from io import BytesIO
from pathlib import Path

from quackpack.util.errors import QuackPackError

# FIXME: Asyncify these functions.


def unpack_to_file(*, source: Path, destination: Path) -> None:
    """
    Unpack all files from `source` to `destination`.
    ----
    Args:
    - `source`: file or directory to be packed.
    - `destination`: directory, which is to be populated with unpacked files.
    """
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(source, mode="r:gz") as f:
        f.extractall(path=destination)


def unpack_to_memory(*, source: Path, file_name: str) -> bytes:
    """
    Unpack all files from `source` to memory.
    ----
    Args:
    - `source`: file or directory to be packed.
    - `file_name`: name of the specific file to be unpacked.
    ----
    Returns:
    - `bytes`: raw data of unpacked file.
    ----
    Raises:
    - `QuackPackError`: if `file_name` is not in given tar.
    """
    with tarfile.open(source, mode="r:gz") as tar:
        if file_name not in tar.getnames():
            raise QuackPackError(f"No member '{file_name}' in '{source}'")
        with tar.extractfile(file_name) as target:  # pyright: ignore[reportOptionalContextManager]; We checked above, that file is in tar.
            return target.read()


def pack_to_memory(*, source: Path | list[Path]) -> bytes:
    """
    Pack all files from `source` to memory.
    ----
    Args:
    - `source`: file or directory to be packed.
    ----
    Returns:
    - `bytes`: raw data of packed files.
    """
    to_add = [source] if isinstance(source, Path) else source
    with BytesIO() as buffer, tarfile.open(fileobj=buffer, mode="w:gz") as tar:
        for f in to_add:
            tar.add(f)
        return buffer.getvalue()


def pack_to_file(*, source: Path | list[Path], destination: Path) -> None:
    """
    Pack all files from `source` to `destination`.
    ----
    Args:
    - `source`: file or directory to be packed.
    - `destination`: path to the new tar.gz.
    """
    to_add = [source] if isinstance(source, Path) else source
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(destination, "w:gz") as tar:
        for f in to_add:
            tar.add(f)
