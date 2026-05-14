"""
Simple Rich-based TUI reporter for integration tests.
Batch updates after each case completes with an event log and failure details.
"""

from dataclasses import dataclass
from datetime import datetime
from typing import Optional
from collections import defaultdict

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
    def _collect_completed_cases(self):
        """
        Collect all completed cases (passed, failed, disabled) as a set of paths.
        Returns: List of tuples (path, status) where status is 'passed', 'failed', or 'disabled'.
        """
        completed = []
        # Gather from events
        for event in self.events:
            if event.event_type == "case_passed":
                completed.append((f"{event.test_path}/{event.case_name}".strip("/"), "passed"))
            elif event.event_type == "case_failed":
                completed.append((f"{event.test_path}/{event.case_name}".strip("/"), "failed"))
            elif event.event_type == "case_disabled":
                completed.append((f"{event.test_path}/{event.case_name}".strip("/"), "disabled"))
        return completed

    def _build_collapsed_tree(self, completed, in_progress=None):
        """
        Build a collapsed filetree from completed cases.
        For nodes of depth ≤3: collapse when all descendants are complete and none are in-progress.
        Otherwise, show the node itself and recurse into its children so non-collapsed nodes remain visible.
        Returns a list of (path, status) for display.
        """
        started = set()
        completed_set = set()
        status_by_leaf = {}
        start_order = {}
        completion_order = {}

        for event_index, event in enumerate(self.events):
            if event.event_type in ("case_passed", "case_failed", "case_disabled"):
                leaf = f"{event.test_path}/{event.case_name}".strip("/")
                completed_set.add(leaf)
                started.add(leaf)
                completion_order.setdefault(leaf, event_index)
                start_order.setdefault(leaf, event_index)
            elif event.event_type in ("test_started", "node_started"):
                path = event.test_path.strip("/")
                started.add(path)
                start_order.setdefault(path, event_index)

        for path, status in completed:
            status_by_leaf[path] = status

        class Node:
            def __init__(self):
                self.children = {}
                self.status = None
                self.started = False
                self.completed = False

        root = Node()

        def ensure_path(path: str):
            node = root
            prefix_parts = []
            for part in path.split("/"):
                prefix_parts.append(part)
                if part not in node.children:
                    node.children[part] = Node()
                node = node.children[part]
                current_path = "/".join(prefix_parts)
                if current_path in started:
                    node.started = True
                if current_path in completed_set:
                    node.completed = True
            return node

        # Populate tree from completed leaves and started prefixes
        for path in completed_set:
            node = ensure_path(path)
            node.status = status_by_leaf.get(path)
            node.completed = True
            node.started = True

        for path in started:
            ensure_path(path).started = True

        def aggregate_status(node: Node):
            child_states = [aggregate_status(child) for child in node.children.values()]

            if not node.children:
                if node.completed:
                    return node.status or "passed"
                if node.started:
                    return "in_progress"
                return None

            if node.started and not node.completed:
                if "failed" in child_states:
                    return "failed"
                return "in_progress"

            statuses = {s for s in child_states if s is not None}
            if not statuses:
                return "in_progress" if node.started else None
            if "failed" in statuses:
                return "failed"
            if "in_progress" in statuses:
                return "in_progress"
            if "disabled" in statuses and len(statuses) == 1:
                return "disabled"
            if statuses == {"passed"}:
                return "passed"
            if "disabled" in statuses:
                return "passed"
            return "passed"

        def can_collapse(node: Node, depth: int):
            if depth > 3:
                return False

            def all_leaf_descendants_complete(n: Node):
                if not n.children:
                    return n.completed
                return all(all_leaf_descendants_complete(child) for child in n.children.values())

            def any_leaf_descendant_in_progress(n: Node):
                if not n.children:
                    return n.started and not n.completed
                return any(any_leaf_descendant_in_progress(child) for child in n.children.values())

            return node.children and all_leaf_descendants_complete(node) and not any_leaf_descendant_in_progress(node)

        def collapsed_status(node: Node):
            leaf_statuses = []

            def gather(n: Node):
                if not n.children:
                    if n.completed and n.status is not None:
                        leaf_statuses.append(n.status)
                    return
                for child in n.children.values():
                    gather(child)

            gather(node)
            if "failed" in leaf_statuses:
                return "failed"
            if "disabled" in leaf_statuses and len(set(leaf_statuses)) == 1:
                return "disabled"
            return "passed"

        def collapsed_order(node: Node, prefix_parts: list[str]):
            orders = []

            def gather(n: Node, current_parts: list[str]):
                if not n.children:
                    if n.completed:
                        path = "/".join(current_parts)
                        orders.append(completion_order.get(path, len(self.events)))
                    return
                for name, child in n.children.items():
                    gather(child, current_parts + [name])

            gather(node, prefix_parts)
            return max(orders) if orders else len(self.events)

        result = []

        def walk(node: Node, prefix_parts: list[str], depth: int):
            path = "/".join(prefix_parts)
            if prefix_parts and can_collapse(node, depth):
                result.append((path, collapsed_status(node), collapsed_order(node, prefix_parts)))
                return

            # Only append leaf nodes (not internal nodes with children)
            if prefix_parts and path != "integration_tests" and not node.children:
                status = aggregate_status(node)
                if status is not None:
                    order = completion_order.get(path, len(self.events))
                    if status == "in_progress":
                        order = start_order.get(path, len(self.events) + 1)
                        if order == len(self.events):
                            order = len(self.events) + 1
                    result.append((path, status, order))

            if not node.children:
                return

            for name, child in node.children.items():
                walk(child, prefix_parts + [name], depth + 1)

        for name, child in root.children.items():
            walk(child, [name], 1)

        seen = set()
        final = []
        if in_progress and in_progress != "integration_tests":
            covered = any(path == in_progress or path.startswith(in_progress + "/") for path, _, _ in result)
            if not covered:
                result.append((in_progress, "in_progress", len(self.events) + 1))
        for path, status, order in result:
            if path and path not in seen:
                final.append((path, status, order))
                seen.add(path)

        final.sort(key=lambda x: x[2])
        return [(path, status) for path, status, _ in final]

    def __init__(self):
        self.console = Console() if HAS_RICH else None
        self.events: list[Event] = []
        self.failures: list[FailureDetail] = []
        self.passed_count = 0
        self.failed_count = 0
        self.disabled_count = 0
        self.current_test = ""
        self.max_events_shown = 20

    def _strip_prefix(self, path: str) -> str:
        """Strip 'integration_tests/' prefix from paths for compact display."""
        prefix = "integration_tests/"
        if path.startswith(prefix):
            return path[len(prefix):]
        return path

    def on_test_start(self, test_path: str):
        """Called when a test starts."""
        short_path = self._strip_prefix(test_path)
        self.current_test = short_path
        self._add_event("test_started", short_path, "", f"Starting test")

    def on_status(self, details: str):
        """Called for high-level status updates."""
        self._add_event("status", self.current_test, "", details)

    def on_node_start(self, node_path: str):
        """Called when a test node traversal begins."""
        short_path = self._strip_prefix(node_path)
        self._add_event("node_started", short_path, "", "Entering node")
        # Only render update if node depth <= 4
        # Depth = number of '/' in short_path + 1 (if not empty)
        depth = short_path.count("/") + (1 if short_path else 0)
        if depth <= 4:
            self._render_update()

    def on_case_passed(self, test_path: str, case_name: str):
        """Called when a case passes."""
        self.passed_count += 1
        short_path = self._strip_prefix(test_path)
        self._add_event("case_passed", short_path, case_name, "✓ Passed")

    def on_case_failed(self, test_path: str, case_name: str, error_msg: str,
                       stdout_expected: Optional[bytes] = None,
                       stdout_actual: Optional[bytes] = None,
                       stderr_expected: Optional[bytes] = None,
                       stderr_actual: Optional[bytes] = None):
        """Called when a case fails."""
        self.failed_count += 1
        short_path = self._strip_prefix(test_path)
        self._add_event("case_failed", short_path, case_name, f"✗ Failed: {error_msg}")
        # Store failure details for later display
        failure = FailureDetail(
            test_path=short_path,
            case_name=case_name,
            error_msg=error_msg,
            stdout_expected=stdout_expected,
            stdout_actual=stdout_actual,
            stderr_expected=stderr_expected,
            stderr_actual=stderr_actual,
        )
        self.failures.append(failure)

    def on_case_disabled(self, test_path: str, case_name: str):
        """Called when a case is disabled."""
        self.disabled_count += 1
        short_path = self._strip_prefix(test_path)
        self._add_event("case_disabled", short_path, case_name, "⊘ Disabled")

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

        from rich.columns import Columns

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

        # Collapsed completed tests table
        completed = self._collect_completed_cases()
        collapsed = self._build_collapsed_tree(completed)
        completed_table = Table(title="Completed Tests", show_header=True, header_style="bold")
        completed_table.add_column("Test Node", width=100)
        completed_table.add_column("Status", width=15)
        for path, status in collapsed:
            if not path:
                display_path = "(root)"
            else:
                # Show the full collapsed path for clarity
                display_path = path
            if status == "failed":
                style = "red"
                status_text = Text("Failed", style=style)
            elif status == "passed" or status == "disabled":
                style = "green"
                status_text = Text("Passed", style=style) if status == "passed" else Text("Disabled", style="yellow")
            elif status == "in_progress":
                style = "blue"
                status_text = Text("In Progress", style=style)
            else:
                style = "white"
                status_text = Text(str(status), style=style)
            completed_table.add_row(display_path, status_text)

        # Failures table (not detailed diffs)
        failures_table = None
        if self.failures:
            failures_table = Table(title=f"Failures ({len(self.failures)})", show_header=True, header_style="bold red")
            failures_table.add_column("#", width=4)
            failures_table.add_column("Test/Case")  # No width limit, show full path
            failures_table.add_column("Error", width=50)

            max_visible = 16
            n = len(self.failures)
            if n <= max_visible:
                for idx, failure in enumerate(self.failures, 1):
                    test_case = f"{failure.test_path}/{failure.case_name}".lstrip("/")
                    failures_table.add_row(str(idx), test_case, failure.error_msg)
            else:
                # Show first 10, ellipsis, last 6
                first = self.failures[:10]
                last = self.failures[-6:]
                for idx, failure in enumerate(first, 1):
                    test_case = f"{failure.test_path}/{failure.case_name}".lstrip("/")
                    failures_table.add_row(str(idx), test_case, failure.error_msg)
                # Ellipsis row
                failures_table.add_row("...", "... ({} more) ...".format(n - 16), "...")
                for idx, failure in enumerate(last, n - 6 + 1):
                    test_case = f"{failure.test_path}/{failure.case_name}".lstrip("/")
                    failures_table.add_row(str(idx), test_case, failure.error_msg)

        # Render side by side if failures exist, else just completed
        if completed_table and failures_table:
            self.console.print(Columns([completed_table, failures_table], expand=True, equal=True))
        elif completed_table:
            self.console.print(completed_table)
        elif failures_table:
            self.console.print(failures_table)

        self.console.print()

    def _render_plain(self):
        """Fallback plain text rendering without Rich."""
        print(
            f"\n[DIT] Passed: {self.passed_count} | Failed: {self.failed_count} | Disabled: {self.disabled_count}"
        )

    def _render_failures(self):
        """Render the failures panel with diffs (detailed view, not shown in main update)."""
        if not self.console or not HAS_RICH:
            return

        # Only render detailed diffs, not the failures table (now shown in main update)
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

        # Force one last redraw so the final tree/table reflects the finished run.
        self._render_update()
