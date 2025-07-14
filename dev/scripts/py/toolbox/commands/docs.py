from ..impl.docs import docs_impl
from click import command, option

@command()
@option(
	"-b",
	"--build_dir",
	prompt="build directory with docs enabled",
	help="The name of the build directory with enabled docs.",
	default="build",
)
def docs(*args, **kwargs):
    """Builds a documentation for the project and opens it in the browser"""
    docs_impl(*args, **kwargs)
