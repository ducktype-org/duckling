from ..impl.init import init_impl
from click import command


@command()
def init():
    """A general repo setup, performs downloading of submodules and binaries, creates a python venv, etc..."""
    init_impl()
