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
        # Simpler prefix-based collapsing algorithm:
        # - Build mappings of completed leaves and their statuses
        # - For each prefix up to depth 3, collect leaves under it
        # - Collapse the highest-level prefix (smallest depth) when none of its leaves are in-progress
        completed_leaves = [p for p, _ in completed]
        leaf_status = {p: s for p, s in completed}

        # Map prefix -> set(leaves)
        from collections import defaultdict
        leaves_by_prefix = defaultdict(set)
        for leaf in completed_leaves:
            parts = leaf.split("/")
            for d in range(1, min(3, len(parts)) + 1):
                prefix = "/".join(parts[:d])
                leaves_by_prefix[prefix].add(leaf)

        # Determine in-progress leaves (started but not completed)
        in_progress_leaves = {s for s in started if s not in completed_set}

        collapsed_prefixes = []
        covered_leaves = set()

        # Consider prefixes sorted by depth (shallow first)
        prefixes = sorted(leaves_by_prefix.keys(), key=lambda x: x.count("/"))
        for prefix in prefixes:
            leaves = leaves_by_prefix[prefix]
            # Skip if these leaves are already covered by a higher-level collapse
            if leaves.issubset(covered_leaves):
                continue
            # Skip if any leaf under this prefix is in-progress
            if any(l in in_progress_leaves for l in leaves):
                continue
            # Collapse this prefix: decide status
            statuses = {leaf_status.get(l) for l in leaves}
            if "failed" in statuses:
                status = "failed"
            elif "disabled" in statuses:
                status = "disabled"
            else:
                status = "passed"
            collapsed_prefixes.append((prefix, status))
            covered_leaves.update(leaves)

        # Leaves that are not covered by any collapsed prefix should be shown individually
        remaining = [(l, leaf_status[l]) for l in completed_leaves if l not in covered_leaves]

        # Result = collapsed prefixes + remaining leaves (stable order)
        result = collapsed_prefixes + remaining
                if not all_descendants_started(v, prefix + [k], depth + 1):
                    return False
            # If this is a leaf, check if it's in started
            if not [k for k in node if k != "__status__"]:
                leaf_path = "/".join(prefix)
                return leaf_path in started
            return True

        def all_descendants_are_leaves_and_complete(n):
            children = [k for k in n if k != "__status__"]
            if not children:
                return "__status__" in n
            for k in children:
                if not all_descendants_are_leaves_and_complete(n[k]):
                    return False
            return True

        def gather_all_statuses(n, statuses):
            children = [k for k in n if k != "__status__"]
            if not children and "__status__" in n:
                statuses.add(n["__status__"])
            for k in children:
                gather_all_statuses(n[k], statuses)

        def collapse(node, prefix, depth):
            children = [k for k in node if k != "__status__"]
            # If this is a leaf, always show it
            if not children and "__status__" in node:
                return [("/".join(prefix), node["__status__"])]

            # Collapse internal nodes of depth <=3 if ALL descendants are complete
            # and there are no started-but-not-completed descendants (in-progress).
            # Do not collapse the root (empty prefix) — collapse only non-empty prefixes
            def any_started_not_completed_prefix(pref):
                # Check started set for any path under this prefix that is not yet completed
                if not pref:
                    return any(s not in completed_set for s in started)
                base = pref + "/"
                for s in started:
                    if s == pref or s.startswith(base):
                        if s not in completed_set:
                            return True
                return False

            if depth <= 3 and children and prefix:
                if all_descendants_complete(node, prefix, depth) and not any_started_not_completed_prefix("/".join(prefix)):
                    statuses = set()
                    gather_all_statuses(node, statuses)
                    if "failed" in statuses:
                        status = "failed"
                    elif "disabled" in statuses:
                        status = "disabled"
                    else:
                        status = "passed"
                    return [("/".join(prefix), status)]

            # Otherwise, show completed leaves under this node
            rows = []
            for k in node:
                if k == "__status__":
                    continue
                rows.extend(collapse(node[k], prefix + [k], depth + 1))
            return rows

        # Start collapsing from the root
        result = collapse(tree, [], 1)

        # Add in-progress node if provided
        if in_progress:
            result.append((in_progress, "in_progress"))

        # Remove duplicates and sort for stable display
        seen = set()
        final = []
        for path, status in result:
            if path and (path, status) not in seen:
                final.append((path, status))
                seen.add((path, status))
        return final

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
        completed_table.add_column("Test Node")
        completed_table.add_column("Status", width=10)
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
