import argparse


def get_parser(name: str) -> argparse.ArgumentParser:
    """
    Get the argument parser for one external subcommand.
    ----
    Args:
    - `name`: Name of external executable. Note that it must be in format `qp-{name}`, and argument should be stripped from `qp-` prefix.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for specified external subcommand.
    """
    parser = argparse.ArgumentParser(prog=name, description=f"Run external cmd 'qp-{name}'", add_help=False)
    return parser
