from dataclasses import dataclass

from ...helpers import get_dev_directory, log_warning


@dataclass
class ConcurrentItestConfig:
    copy_count: int = 3
    blacklist: list[str] = None

    def __post_init__(self):
        if self.blacklist is None:
            self.blacklist = []


def load_default_concurrent_config() -> ConcurrentItestConfig:

    # Keep defaults resilient: if config is missing or malformed, fall back to safe defaults
    # and continue execution.

    default_config_path = get_dev_directory() / "integration_tests" / "concurrent_itest.yaml"
    if not default_config_path.exists():
        return ConcurrentItestConfig()

    try:
        import yaml

        with open(default_config_path) as file:
            loaded = yaml.safe_load(file.read()) or {}
    except Exception as error:
        log_warning(
            f"Failed to parse '{default_config_path}'. Using concurrent defaults. Error: {error}"
        )
        return ConcurrentItestConfig()

    blacklist = loaded.get("blacklist", [])
    if blacklist is None:
        blacklist = []
    elif not isinstance(blacklist, list) or not all(isinstance(item, str) for item in blacklist):
        log_warning(
            f"Invalid 'blacklist' in '{default_config_path}'. Falling back to empty blacklist."
        )
        blacklist = []

    copy_count = loaded.get("copy_count", 3)
    if not isinstance(copy_count, int) or copy_count < 1:
        log_warning(
            f"Invalid 'copy_count' in '{default_config_path}'. Falling back to 3."
        )
        copy_count = 3

    return ConcurrentItestConfig(copy_count=copy_count, blacklist=blacklist)
