import questionary
from pathlib import Path
from typing import Set, Tuple, List, Optional
from .test_loader import TestNode, Test

def prompt_main_menu(has_failed_tests: bool) -> Optional[str]:
    """Prompt the user for the main interactive menu action."""
    choices = []
    if has_failed_tests:
        choices.append("Run Failed Tests (.last_failed)")
    choices.extend([
        "Run All Tests",
        "Select Tests to Run",
        "Exit"
    ])
    
    return questionary.select(
        "What would you like to run?",
        choices=choices
    ).ask()


def prompt_run_flags(clean: bool, dry: bool, fail_fast: bool, verbose: bool, tui: bool) -> Optional[Tuple[bool, bool, bool, bool, bool]]:
    choices = [
        questionary.Choice("Fail Fast (Stop on first failure)", checked=fail_fast, value="fail_fast"),
        questionary.Choice("Verbose Output (Print raw commands)", checked=verbose, value="verbose"),
        questionary.Choice("Clean (Run clean logic before tests)", checked=clean, value="clean"),
        questionary.Choice("Dry Run (Don't execute)", checked=dry, value="dry"),
        questionary.Choice("TUI Reporter (Rich UI)", checked=tui, value="tui"),
    ]
    
    ans = questionary.checkbox(
        "Configure Run Options:",
        choices=choices
    ).ask()
    
    if ans is None:
        return None
        
    return (
        "clean" in ans,
        "dry" in ans,
        "fail_fast" in ans,
        "verbose" in ans,
        "tui" in ans
    )
