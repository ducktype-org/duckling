import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `login` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `login` command.
    """
    parser = argparse.ArgumentParser(prog="login", description="Login to the registry")
    return parser
