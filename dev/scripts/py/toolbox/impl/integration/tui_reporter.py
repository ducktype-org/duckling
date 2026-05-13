"""
Simple Rich-based TUI reporter for integration tests.
Batch updates after each case completes with an event log and failure details.
"""

from dataclasses import dataclass
from datetime import datetime
from typing import Optional

try:
    from rich.console import Console
    from rich.table import Table
    from rich.panel import Panel
    from rich.text import Text
    HAS_RICH = True
except ImportError:
    HAS_RICH = False


@dataclass
class FailureDetail:
    """Store details of a test failure."""
    test_path: str
    case_name: str
    error_msg: str
    stdout_expected: Optional[bytes] = None
    stdout_actual: Optional[bytes] = None
    stderr_expected: Optional[bytes] = None
    stderr_actual: Optional[bytes] = None


@dataclass
class Event:
    """A single event in the test execution."""
    timestamp: datetime
    event_type: str  # "case_passed", "case_failed", "case_disabled", "test_started"
    test_path: str
    case_name: str = ""
    details: str = ""


class IntegrationTuiReporter:
    """
    Simple batch-mode TUI reporter using Rich.
    Updates after each case completes, shows event log and failures.
    """

    def __init__(self):
        self.console = Console() if HAS_RICH else None
        self.events: list[Event] = []
        self.failures: list[FailureDetail] = []
        self.passed_count = 0
        self.failed_count = 0
        self.disabled_count = 0
        self.current_test = ""
        self.max_events_shown = 20

    def on_test_start(self, test_path: str):
        """Called when a test starts."""
        self.current_test = test_path
        self._add_event("test_started", test_path, "", f"Starting test")
        self._render_update()

    def on_status(self, details: str):
        """Called for high-level status updates."""
        self._add_event("status", self.current_test, "", details)
        self._render_update()

    def on_node_start(self, node_path: str):
        """Called when a test node traversal begins."""
        self._add_event("node_started", node_path, "", "Entering node")
        self._render_update()

    def on_case_passed(self, test_path: str, case_name: str):
        """Called when a case passes."""
        self.passed_count += 1
        self._add_event("case_passed", test_path, case_name, "✓ Passed")
        self._render_update()

    def on_case_failed(self, test_path: str, case_name: str, error_msg: str,
                       stdout_expected: Optional[bytes] = None,
                       stdout_actual: Optional[bytes] = None,
                       stderr_expected: Optional[bytes] = None,
                       stderr_actual: Optional[bytes] = None):
        """Called when a case fails."""
        self.failed_count += 1
        self._add_event("case_failed", test_path, case_name, f"✗ Failed: {error_msg}")
        
        # Store failure details for later display
        failure = FailureDetail(
            test_path=test_path,
            case_name=case_name,
            error_msg=error_msg,
            stdout_expected=stdout_expected,
            stdout_actual=stdout_actual,
            stderr_expected=stderr_expected,
            stderr_actual=stderr_actual,
        )
        self.failures.append(failure)
        self._render_update()

    def on_case_disabled(self, test_path: str, case_name: str):
        """Called when a case is disabled."""
        self.disabled_count += 1
        self._add_event("case_disabled", test_path, case_name, "⊘ Disabled")
        self._render_update()

    def _add_event(self, event_type: str, test_path: str, case_name: str, details: str):
        """Add an event to the log."""
        event = Event(
            timestamp=datetime.now(),
            event_type=event_type,
            test_path=test_path,
            case_name=case_name,
            details=details,
        )
        self.events.append(event)

    def _render_update(self):
        """Render a batch update after a case completes."""
        if not self.console or not HAS_RICH:
            self._render_plain()
            return

        self.console.clear()
        
        # Summary header
        summary_text = (
            f"[bold cyan]Integration Tests[/bold cyan] | "
            f"[green]Passed: {self.passed_count}[/green] | "
            f"[red]Failed: {self.failed_count}[/red] | "
            f"[yellow]Disabled: {self.disabled_count}[/yellow]"
        )
        self.console.print(summary_text)
        self.console.print()

        # Recent events table
        if self.events:
            table = Table(title="Recent Events", show_header=True, header_style="bold")
            table.add_column("Time", width=10)
            table.add_column("Type", width=15)
            table.add_column("Test/Case", width=40)
            table.add_column("Status", width=40)

            # Show last N events
            recent = self.events[-self.max_events_shown:]
            for event in recent:
                time_str = event.timestamp.strftime("%H:%M:%S")
                
                # Color code the event type
                type_style = "blue"
                if event.event_type == "case_passed":
                    type_style = "green"
                elif event.event_type == "case_failed":
                    type_style = "red"
                elif event.event_type == "case_disabled":
                    type_style = "yellow"

                event_type_text = Text(event.event_type, style=type_style)
                test_case = f"{event.test_path}/{event.case_name}".lstrip("/")
                
                table.add_row(time_str, event_type_text, test_case, event.details)

            self.console.print(table)
            self.console.print()

        # Failures section
        if self.failures:
            self._render_failures()

    def _render_plain(self):
        """Fallback plain text rendering without Rich."""
        print(
            f"\n[DIT] Passed: {self.passed_count} | Failed: {self.failed_count} | Disabled: {self.disabled_count}"
        )

    def _render_failures(self):
        """Render the failures panel with diffs."""
        if not self.console or not HAS_RICH:
            return

        failures_table = Table(title="Failures", show_header=True, header_style="bold red")
        failures_table.add_column("Test/Case", width=50)
        failures_table.add_column("Error", width=50)

        for failure in self.failures:
            test_case = f"{failure.test_path}/{failure.case_name}".lstrip("/")
            failures_table.add_row(test_case, failure.error_msg)

        self.console.print(failures_table)
        self.console.print()

        # Detailed diffs
        for i, failure in enumerate(self.failures):
            title = f"Failure {i + 1}: {failure.test_path}/{failure.case_name}"
            
            diff_text = f"[bold red]{failure.error_msg}[/bold red]\n"

            if failure.stdout_expected is not None and failure.stdout_actual is not None:
                diff_text += "\n[bold]Expected stdout:[/bold]\n"
                diff_text += self._format_bytes(failure.stdout_expected)
                diff_text += "\n[bold]Actual stdout:[/bold]\n"
                diff_text += self._format_bytes(failure.stdout_actual)

            if failure.stderr_expected is not None and failure.stderr_actual is not None:
                diff_text += "\n[bold]Expected stderr:[/bold]\n"
                diff_text += self._format_bytes(failure.stderr_expected)
                diff_text += "\n[bold]Actual stderr:[/bold]\n"
                diff_text += self._format_bytes(failure.stderr_actual)

            panel = Panel(diff_text, title=title, expand=False)
            self.console.print(panel)

    def _format_bytes(self, data: bytes) -> str:
        """Format bytes for display."""
        try:
            text = data.decode("utf-8", errors="replace")
            # Truncate very long output
            if len(text) > 500:
                text = text[:500] + "\n... [truncated]"
            return text
        except Exception:
            return f"[{len(data)} bytes]"

    def finish(self):
        """Render final summary."""
        if not self.console or not HAS_RICH:
            print(
                f"\n[DIT] Final: Passed: {self.passed_count} | Failed: {self.failed_count} | Disabled: {self.disabled_count}"
            )
            return

        self.console.clear()
        
        # Final summary
        summary_text = (
            f"[bold cyan]Integration Tests Complete[/bold cyan]\n"
            f"[green]Passed: {self.passed_count}[/green] | "
            f"[red]Failed: {self.failed_count}[/red] | "
            f"[yellow]Disabled: {self.disabled_count}[/yellow]"
        )
        self.console.print(Panel(summary_text, expand=False))
        self.console.print()

        # Failures section
        if self.failures:
            self._render_failures()
