import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `logout` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `logout` command.
    """
    parser = argparse.ArgumentParser(prog="logout", description="Logout from the registry")
    return parser
