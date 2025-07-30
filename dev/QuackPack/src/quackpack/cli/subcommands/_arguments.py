import argparse
from dataclasses import dataclass


@dataclass(frozen=True, kw_only=True)
class Arguments:
    matched: argparse.Namespace
