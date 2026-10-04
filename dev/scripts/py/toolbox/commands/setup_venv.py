from ..impl.setup_venv import setup_venv_impl
from click import command


@command()
def setup_venv():
    """Setups python virtual environment, download dependencies"""
    setup_venv_impl()
