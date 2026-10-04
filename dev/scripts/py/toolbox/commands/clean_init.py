from ..impl.clean_init import clean_init_impl

from ..impl.helpers import (
    abort_if_false,
)

from click import command, option


@command()
@option(
    "--yes",
    is_flag=True,
    callback=abort_if_false,
    expose_value=False,
    prompt="This operation deletes files, are you sure?",
)
def clean_init():
    """Removes things created by init."""
    clean_init_impl()
