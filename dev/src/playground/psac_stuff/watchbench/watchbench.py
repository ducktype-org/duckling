#!/usr/bin/env python3
"""
watchbench.py - benchmark harness for watch-mode (in-memory, stateful) compilers.

Linux only, no dependencies. Usage:
    python3 watchbench.py config.json --sessions 10 --iters 40 --warmup 5 \\
        --accounting systemd --perf on --out results.jsonl

Design:
  * Unit of replication = a SESSION: fresh compiler process -> initial build -> warm-up
    edits (flagged, kept in output) -> measured edits -> kill.
  * Sessions of different compilers are interleaved in randomized blocks so machine
    drift (thermal, background load) affects all of them equally. Session i uses the same
    edit order for every compiler.
  * Each trial: write edit (+ trigger line on stdin for "trigger": "stdin" compilers)
    -> wait for the completion marker on stdout (perceived latency) -> wait until the
    whole process tree stops consuming CPU (time to quiescence). Every edit is followed
    by a separately-labelled REVERT trial.
  * A trial that times out aborts its session (a "session_abort" record follows); the
    compiler may still be busy, so later trials of that session would be contaminated.
  * CPU/memory accounting (--accounting) uses a dedicated cgroup v2 per session:
    microsecond CPU time over every thread and descendant, cgroup.kill teardown, and
    memory.peak if the memory controller is available. Modes: create the cgroup directly
    (root or a delegated --cgroup-parent), `systemd-run --user --scope`, or a /proc tree
    walk (10 ms CPU resolution, misses reparented processes). Real runs must pass direct
    or systemd explicitly; auto only exists for trying things out.
  * Hardware counters (--perf) come from perf_event_open on the session's root process,
    opened before it execs the compiler, with inherit=1: every thread and child the
    compiler ever creates is counted, user space only. On hybrid CPUs one counter is
    opened per core PMU (e.g. cpu_core + cpu_atom) and the counts are summed.
  * Edits may contain "{n}", replaced by a globally unique counter, so a compiler
    cannot answer from a memoized previous result for identical content.
  * The first output line of every invocation is a "meta" record describing the machine,
    the harness arguments, the config and the hashes of the binaries in each command.

Config (JSON):
{
  "project": "/path/to/project",
  "reset_cmd": ["git", "checkout", "--", "."],
  "compilers": [
    {"name": "A", "cmd": ["compA", "main.src", "-i", "-j", "4"],
     "cpus": "0,1,3,6",
     "trigger": "stdin", "trigger_line": "",
     "done_regex": "^(recompiled \\\\d+/\\\\d+ functions|compilation failed:)",
     "error_regex": "^compilation failed:",
     "stats_regex": "recompiled (?P<recompiled>\\\\d+)/(?P<total>\\\\d+)",
     "env": {}}
  ],
  "edits": [
    {"name": "body_leaf", "file": "src/leaf.x", "find": "return 1;", "replace": "return {n};"},
    {"name": "sig_hub", "file": "src/hub.x",
     "replacements": [{"find": "fun f(a: i64)", "replace": "fun f(a: i32)"},
                      {"find": "f(1i64)", "replace": "f(1i32)"}]},
    {"name": "add_fn", "file": "src/leaf.x", "find": "fun g(",
     "replace": "fun wb_{n}() -> i64 = {\\n\\treturn {n}i64;\\n}\\n\\nfun g("},
    {"name": "remove_fn", "file": "src/leaf.x", "regex": true,
     "find": "^fun unused_helper\\(.*?^}\\n", "replace": "# removed {n}\\n"}
  ]
}
  cpus          optional; the command is wrapped in `taskset -c <cpus>`
  trigger       "file" (default: the compiler watches the file itself) or "stdin"
                (after the write, trigger_line + newline is written to its stdin)
  stats_regex   optional; named groups of the matched done line become numeric fields
  regex         on an edit or a replacement: find is a Python regex (MULTILINE | DOTALL,
                first match only; replace may use \\1-style groups). The revert restores the
                file, so "add_fn" removes the function again and "remove_fn" brings it back.
                Leave a {n} in removal replacements so the edited content stays unique.
"""

import argparse
import ctypes
import datetime
import hashlib
import json
import os
import platform
import pty
import queue
import random
import re
import shutil
import signal
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

CLK_TCK = os.sysconf("SC_CLK_TCK")  # usually 100 -> 10 ms CPU-time resolution
PAGE = os.sysconf("SC_PAGE_SIZE")
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\x1b[()][A-Z0-9]|\x1bc")


class SessionAborted(Exception):
    pass


# ---------------------------------------------------------------- accounting
def _stat(pid):
    # Fields after the ")" of comm; index = (man 5 proc field number) - 3
    return Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()


def proc_tree(root):
    kids = {}
    for p in Path("/proc").iterdir():
        if p.name.isdigit():
            try:
                kids.setdefault(int(_stat(p.name)[1]), []).append(int(p.name))
            except (OSError, IndexError):
                pass
    out, stack = [], [root]
    while stack:
        pid = stack.pop()
        out.append(pid)
        stack.extend(kids.get(pid, []))
    return out


def proc_cpu_rss(root):
    """Fallback: user+sys CPU s (10 ms ticks, per-process truncation) and RSS bytes."""
    cpu, rss = 0.0, 0
    for pid in proc_tree(root):
        try:
            f = _stat(pid)
            cpu += (int(f[11]) + int(f[12]) + int(f[13]) + int(f[14])) / CLK_TCK
            rss += int(f[21]) * PAGE
        except (OSError, IndexError):
            pass
    return cpu, rss


def cgroup2_mount():
    for line in Path("/proc/self/mountinfo").read_text().splitlines():
        pre, post = line.split(" - ", 1)
        if post.split()[0] == "cgroup2":
            return Path(pre.split()[4])
    return None


def _systemd_user_ok():
    try:
        return (
            subprocess.run(
                ["systemd-run", "--user", "--scope", "--quiet", "--collect", "true"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=10,
            ).returncode
            == 0
        )
    except (OSError, subprocess.TimeoutExpired):
        return False


def choose_accounting(requested, parent):
    """Returns (mode, cgroup parent dir or None). Modes: direct | systemd | proc."""
    mount = cgroup2_mount()
    if requested in ("auto", "direct") and mount:
        p = Path(parent) if parent else mount
        try:
            probe = p / f"watchbench-probe-{os.getpid()}"
            probe.mkdir()
            probe.rmdir()
            return "direct", p
        except OSError:
            if requested == "direct":
                raise SystemExit(
                    f"cannot create cgroups under {p} (need root or delegation)"
                )
    if requested in ("auto", "systemd") and mount and _systemd_user_ok():
        return "systemd", mount
    if requested in ("systemd", "direct"):
        raise SystemExit(f"--accounting {requested} unavailable on this machine")
    if requested == "auto":
        print(
            "WARNING: cgroup v2 accounting unavailable; falling back to /proc "
            "(10 ms CPU resolution, misses reparented processes)",
            file=sys.stderr,
        )
    return "proc", None


class Accounting:
    """CPU/memory accounting for one compiler session."""

    def __init__(self, mode, parent, name):
        self.mode, self.cg, self.parent, self.name = mode, None, parent, name
        if mode == "direct":
            self.cg = parent / name
            self.cg.mkdir()

    def wrap(self, cmd):
        if (
            self.mode == "direct"
        ):  # join the cgroup before exec -> all descendants inherit it
            return [
                "sh",
                "-c",
                'echo $$ > "$1/cgroup.procs" && shift && exec "$@"',
                "sh",
                str(self.cg),
                *cmd,
            ]
        if self.mode == "systemd":
            return [
                "systemd-run",
                "--user",
                "--scope",
                "--quiet",
                "--collect",
                f"--unit={self.name}",
                "--",
                *cmd,
            ]
        return cmd

    def attach(self, pid, timeout_s=10):
        self.pid = pid
        if self.mode != "systemd":
            return
        deadline = (
            time.monotonic() + timeout_s
        )  # wait until systemd has moved pid into scope
        while time.monotonic() < deadline:
            for line in Path(f"/proc/{pid}/cgroup").read_text().splitlines():
                if line.startswith("0::") and line.endswith(f"/{self.name}.scope"):
                    self.cg = self.parent / line[3:].lstrip("/")
                    return
            time.sleep(0.01)
        raise RuntimeError("systemd scope did not appear")

    def _read_kv(self, fname):
        try:
            return dict(l.split() for l in (self.cg / fname).read_text().splitlines())
        except OSError:
            return {}

    def _read_int(self, fname):
        try:
            return int((self.cg / fname).read_text().split()[0])
        except (OSError, ValueError, IndexError):
            return None

    def cpu(self):
        """Total CPU seconds of the whole session so far."""
        if self.cg is None:
            return proc_cpu_rss(self.pid)[0]
        return int(self._read_kv("cpu.stat").get("usage_usec", 0)) / 1e6

    def snapshot(self):
        """Detailed readings for a trial record."""
        if self.cg is None:
            cpu, rss = proc_cpu_rss(self.pid)
            return {"cpu_s": cpu, "rss_mb": rss / 2**20}
        st = self._read_kv("cpu.stat")
        d = {
            "cpu_s": int(st.get("usage_usec", 0)) / 1e6,
            "user_s": int(st.get("user_usec", 0)) / 1e6,
            "sys_s": int(st.get("system_usec", 0)) / 1e6,
            "rss_mb": proc_cpu_rss(self.pid)[1] / 2**20,
        }
        cur, peak = self._read_int("memory.current"), self._read_int("memory.peak")
        if cur is not None:  # only with the memory controller; includes page cache
            d["cg_mem_mb"] = cur / 2**20
        if peak is not None:  # session high-water mark (kernel 5.19+)
            d["cg_mem_peak_mb"] = peak / 2**20
        return d

    def kill(self):
        if self.cg is not None and (self.cg / "cgroup.kill").exists():
            try:
                (self.cg / "cgroup.kill").write_text(
                    "1"
                )  # kills every member, incl. escapees
            except OSError:
                pass

    def cleanup(self):
        if self.mode != "direct":
            return  # systemd --collect removes the scope itself
        for _ in range(200):
            try:
                self.cg.rmdir()
                return
            except OSError:
                time.sleep(0.05)


def wait_quiescent(acct, window_s=0.3, poll_s=0.02, idle_cores=0.02, timeout_s=120):
    """Return perf_counter_ns of the last poll interval in which the session used more
    than `idle_cores` CPU, once it has stayed below that for `window_s`; None on timeout.
    The threshold tolerates watchers that poll the filesystem while idle."""
    last_cpu, last_t = acct.cpu(), time.perf_counter_ns()
    last_busy = last_t
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        time.sleep(poll_s)
        cpu, now = acct.cpu(), time.perf_counter_ns()
        if (cpu - last_cpu) / max((now - last_t) / 1e9, 1e-9) > idle_cores:
            last_busy = now
        elif (now - last_busy) / 1e9 >= window_s:
            return last_busy
        last_cpu, last_t = cpu, now
    return None


# ---------------------------------------------------------------- hardware counters
PERF_TYPE_HARDWARE = 0
PERF_HW = {
    "cycles": 0,
    "instructions": 1,
    "cache-references": 2,
    "cache-misses": 3,
    "branches": 4,
    "branch-misses": 5,
}
PERF_FORMAT_TOTAL_TIME_ENABLED, PERF_FORMAT_TOTAL_TIME_RUNNING = 1, 2
PERF_ATTR_FLAGS = (
    (1 << 0)  # disabled until ...
    | (1 << 1)  # inherit: every thread and child created afterwards
    | (1 << 5)  # exclude_kernel
    | (1 << 6)  # exclude_hv
    | (1 << 12)  # enable_on_exec: ... the gate shell execs the real command
)
PERF_FLAG_FD_CLOEXEC = 8
NR_PERF_EVENT_OPEN = {"x86_64": 298, "aarch64": 241, "riscv64": 241}


def core_pmus():
    """[(name, type)] of the core PMUs on a hybrid CPU, [] on a homogeneous one."""
    out = []
    for d in sorted(Path("/sys/bus/event_source/devices").glob("*")):
        if (d / "cpus").exists() and (d / "type").exists():
            out.append((d.name, int((d / "type").read_text())))
    return out


class PerfCounters:
    """User-space hardware counters over a process and everything it ever spawns.
    Reading an inherited counter sums the live children, and exited children are
    folded into the parent, so readings grow monotonically for the whole session."""

    _libc = None

    def __init__(self, pid, events):
        if PerfCounters._libc is None:
            PerfCounters._libc = ctypes.CDLL(None, use_errno=True)
            PerfCounters._libc.syscall.restype = ctypes.c_long
        nr = NR_PERF_EVENT_OPEN.get(platform.machine())
        if nr is None:
            raise OSError(
                f"perf_event_open: unknown syscall number on {platform.machine()}"
            )
        pmus = core_pmus() or [(None, 0)]
        self.fds = {}
        try:
            for ev in events:
                for _, pmu_type in pmus:
                    config = (pmu_type << 32) | PERF_HW[ev]
                    attr = struct.pack(
                        "=IIQQQQQIIQ",
                        PERF_TYPE_HARDWARE,
                        64,
                        config,
                        0,
                        0,
                        PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING,
                        PERF_ATTR_FLAGS,
                        0,
                        0,
                        0,
                    )
                    buf = ctypes.create_string_buffer(attr, len(attr))
                    fd = self._libc.syscall(
                        nr, buf, ctypes.c_int(pid), ctypes.c_int(-1), ctypes.c_int(-1),
                        ctypes.c_ulong(PERF_FLAG_FD_CLOEXEC),
                    )  # fmt: skip
                    if fd < 0:
                        err = ctypes.get_errno()
                        raise OSError(err, f"perf_event_open({ev}): {os.strerror(err)}")
                    self.fds.setdefault(ev, []).append(fd)
        except BaseException:
            self.close()
            raise

    def read(self):
        """{event: count}, plus "perf_multiplexed": True if any counter was descheduled."""
        out, multiplexed = {}, False
        for ev, fds in self.fds.items():
            total = 0
            for fd in fds:
                value, enabled, running = struct.unpack("=QQQ", os.read(fd, 24))
                if running and running < enabled:
                    multiplexed = True
                    value = value * enabled // running
                total += value
            out[ev] = total
        if multiplexed:
            out["perf_multiplexed"] = True
        return out

    def close(self):
        for fds in self.fds.values():
            for fd in fds:
                os.close(fd)
        self.fds = {}


def probe_perf(events):
    """Opens the counters on a short-lived child; returns None or the error text."""
    r, w = os.pipe()
    p = subprocess.Popen(
        ["sh", "-c", "read _ <&3; exec 3<&-; exec true"], pass_fds=(r,)
    )
    try:
        PerfCounters(p.pid, events).close()
        return None
    except OSError as e:
        return str(e)
    finally:
        os.write(w, b"\n")
        os.close(r)
        os.close(w)
        p.wait()


# ---------------------------------------------------------------- compiler process
GATE = ["sh", "-c", 'read _ <&3; exec 3<&-; exec "$@"', "sh"]


class Watcher:
    """Runs the compiler on a pty (prevents stdout block-buffering) and timestamps
    every output line as soon as it is read. With perf events, the command starts
    behind a gate shell so the counters exist before the compiler's first instruction.
    """

    def __init__(self, cmd, cwd, env, stdin_trigger, perf_events, on_spawn):
        master, slave = pty.openpty()
        gate_r, gate_w = os.pipe() if perf_events else (None, None)
        self.proc = subprocess.Popen(
            GATE + cmd if perf_events else cmd,
            cwd=cwd,
            env={**os.environ, **env},
            stdin=subprocess.PIPE if stdin_trigger else subprocess.DEVNULL,
            stdout=slave,
            stderr=slave,
            pass_fds=(gate_r,) if perf_events else (),
            start_new_session=True,
            bufsize=0,
        )
        os.close(slave)
        self.perf = None
        try:
            if perf_events:
                self.perf = PerfCounters(self.proc.pid, perf_events)
        finally:
            if perf_events:
                os.write(gate_w, b"\n")
                os.close(gate_r)
                os.close(gate_w)
        on_spawn(self.proc.pid)
        self.lines = queue.Queue()
        threading.Thread(target=self._reader, args=(master,), daemon=True).start()

    def _reader(self, fd):
        buf = b""
        while True:
            try:
                chunk = os.read(fd, 65536)
            except OSError:
                break
            if not chunk:
                break
            t = time.perf_counter_ns()
            buf += chunk
            *done, buf = buf.split(b"\n")
            for raw in done:
                self.lines.put(
                    (t, ANSI.sub("", raw.decode(errors="replace")).strip("\r "))
                )

    def drain(self):
        out = []
        while not self.lines.empty():
            out.append(self.lines.get_nowait())
        return out

    def trigger(self, line):
        self.proc.stdin.write(line.encode() + b"\n")

    def wait_for(self, regex, timeout_s):
        """Return (timestamp_ns or None, lines seen)."""
        seen, deadline = [], time.monotonic() + timeout_s
        while (left := deadline - time.monotonic()) > 0:
            try:
                t, line = self.lines.get(timeout=left)
            except queue.Empty:
                break
            seen.append(line)
            if regex.search(line):
                return t, seen
        return None, seen

    def counters(self):
        return self.perf.read() if self.perf else {}

    def stop(self, acct):
        if self.proc.stdin:
            try:
                self.proc.stdin.close()  # EOF ends a stdin-driven loop gracefully
                self.proc.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                pass
        if self.proc.poll() is None:
            try:
                os.killpg(self.proc.pid, signal.SIGTERM)
            except (ProcessLookupError, PermissionError):
                pass
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
        acct.kill()  # cgroup.kill: also catches reparented/double-forked processes
        if self.proc.poll() is None:
            try:
                os.killpg(self.proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            self.proc.wait()
        if self.perf:
            self.perf.close()
        acct.cleanup()


# ---------------------------------------------------------------- edits
def write_file(path, text, atomic):
    if atomic:  # what most editors do: write temp file, rename over original
        tmp = path.with_name(f".{path.name}.wbtmp")
        with open(tmp, "w") as f:
            f.write(text)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    else:
        with open(path, "w") as f:
            f.write(text)
            f.flush()
            os.fsync(f.fileno())


def edit_replacements(e):
    if "replacements" in e:
        return e["replacements"]
    return [{k: e[k] for k in ("find", "replace", "regex") if k in e}]


def apply_edit(e, original, n):
    text = original
    for r in edit_replacements(e):
        replace = r["replace"].replace("{n}", str(n))
        if r.get("regex"):
            text, hits = re.subn(
                r["find"], replace, text, count=1, flags=re.MULTILINE | re.DOTALL
            )
        else:
            hits = int(r["find"] in text)
            text = text.replace(r["find"], replace, 1)
        if not hits:
            raise ValueError(f"edit {e['name']}: pattern not found: {r['find']!r}")
    return text


def edit_order(edits, count, rng, mix):
    if mix == "random":
        return [rng.choice(edits) for _ in range(count)]
    out = []  # stratified: every edit once per pass, shuffled within the pass
    while len(out) < count:
        block = edits[:]
        rng.shuffle(block)
        out.extend(block)
    return out[:count]


# ---------------------------------------------------------------- metadata
def _read(path):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return None


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def binaries(cmd):
    out = {}
    for arg in cmd:
        path = arg if "/" in arg else shutil.which(arg)
        if path and os.path.isfile(path) and os.access(path, os.X_OK):
            st = os.stat(path)
            out[os.path.realpath(path)] = {
                "sha256": _sha256(path),
                "size": st.st_size,
                "mtime": datetime.datetime.fromtimestamp(st.st_mtime).isoformat(),
            }
    return out


def meta_record(cfg, args, mode, parent, perf_events, perf_error):
    cpu_model = next(
        (
            l.split(":", 1)[1].strip()
            for l in (_read("/proc/cpuinfo") or "").splitlines()
            if l.startswith("model name")
        ),
        platform.processor(),
    )
    cpufreq = "/sys/devices/system/cpu/cpu0/cpufreq"
    return {
        "kind": "meta",
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "host": platform.node(),
        "kernel": platform.release(),
        "machine": platform.machine(),
        "cpu_model": cpu_model,
        "ncpu": os.cpu_count(),
        "core_pmus": dict(core_pmus()),
        "governor": _read(f"{cpufreq}/scaling_governor"),
        "energy_perf_preference": _read(f"{cpufreq}/energy_performance_preference"),
        "intel_no_turbo": _read("/sys/devices/system/cpu/intel_pstate/no_turbo"),
        "cpufreq_boost": _read("/sys/devices/system/cpu/cpufreq/boost"),
        "smt": _read("/sys/devices/system/cpu/smt/control"),
        "perf_event_paranoid": _read("/proc/sys/kernel/perf_event_paranoid"),
        "harness_cpus": sorted(os.sched_getaffinity(0)),
        "accounting": mode,
        "cgroup_parent": str(parent) if parent else None,
        "perf_events": perf_events,
        "perf_error": perf_error,
        "args": vars(args),
        "config": {k: v for k, v in cfg.items() if k != "compilers"}
        | {"compilers": [{k: v for k, v in c.items() if not k.startswith("_")} for c in cfg["compilers"]]},
        "binaries": {c["name"]: binaries(c["cmd"]) for c in cfg["compilers"]},
    }  # fmt: skip


# ---------------------------------------------------------------- main loop
def done_fields(comp, t_done, seen):
    r = {"done_line": seen[-1] if t_done is not None and seen else None}
    if r["done_line"] and comp["_stats_re"]:
        m = comp["_stats_re"].search(r["done_line"])
        if m:
            for k, v in m.groupdict().items():
                if v is not None:
                    r[k] = float(v) if "." in v else int(v)
    return r


def run_trial(w, acct, comp, path, text, args):
    w.drain()  # discard stale output
    s0, c0 = acct.snapshot(), w.counters()
    t0 = time.perf_counter_ns()
    write_file(path, text, args.atomic)
    if comp["_stdin"]:
        w.trigger(comp.get("trigger_line", ""))
    t_done, seen = w.wait_for(comp["_done_re"], args.timeout)
    t_quiet = wait_quiescent(acct, idle_cores=args.idle_cores, timeout_s=args.timeout)
    s1, c1 = acct.snapshot(), w.counters()
    r = {
        "latency_ms": None if t_done is None else (t_done - t0) / 1e6,
        "quiescence_ms": None if t_quiet is None else max(0.0, (t_quiet - t0) / 1e6),
        "error": (
            any(comp["_error_re"].search(l) for l in seen)
            if comp["_error_re"]
            else False
        ),
        "timed_out": t_done is None or t_quiet is None,
    }
    r.update(done_fields(comp, t_done, seen))
    for k in ("cpu_s", "user_s", "sys_s"):  # per-trial deltas
        if k in s1:
            r[k] = s1[k] - s0[k]
    for k in ("rss_mb", "cg_mem_mb", "cg_mem_peak_mb"):  # levels after the trial
        if k in s1:
            r[k] = s1[k]
    for k, v in c1.items():
        r[k] = v if k == "perf_multiplexed" else v - c0.get(k, 0)
    return r


def compiler_cmd(comp):
    return (
        ["taskset", "-c", comp["cpus"], *comp["cmd"]]
        if comp.get("cpus")
        else comp["cmd"]
    )


def run_session(cfg, comp, sid, args, counter, out, mode, parent, perf_events):
    proj = Path(cfg["project"])
    if cfg.get("reset_cmd"):
        subprocess.run(cfg["reset_cmd"], cwd=proj, check=True)
    acct = Accounting(mode, parent, f"watchbench-{comp['name']}-s{sid}-{os.getpid()}")
    base = {"compiler": comp["name"], "session": sid}

    def emit(rec):
        out.write(json.dumps(base | rec) + "\n")
        out.flush()

    t0 = time.perf_counter_ns()
    w = Watcher(
        acct.wrap(compiler_cmd(comp)),
        proj,
        comp.get("env", {}),
        comp["_stdin"],
        perf_events,
        acct.attach,
    )
    try:
        t_init, seen = w.wait_for(comp["_done_re"], args.timeout)
        t_quiet = wait_quiescent(
            acct, idle_cores=args.idle_cores, timeout_s=args.timeout
        )
        rec = {
            "kind": "initial",
            "accounting": mode,
            "latency_ms": None if t_init is None else (t_init - t0) / 1e6,
            "quiescence_ms": (
                None if t_quiet is None else max(0.0, (t_quiet - t0) / 1e6)
            ),
            "timed_out": t_init is None or t_quiet is None,
        }
        rec.update(done_fields(comp, t_init, seen))
        rec.update(acct.snapshot())
        rec.update(w.counters())
        emit(rec)
        if rec["timed_out"]:
            raise SessionAborted("initial build timed out")
        rng = random.Random(f"{args.seed}-{sid}")  # same edit order for every compiler
        for i, e in enumerate(
            edit_order(cfg["edits"], args.warmup + args.iters, rng, args.mix)
        ):
            path = proj / e["file"]
            original = path.read_text()
            counter[0] += 1
            edited = apply_edit(e, original, counter[0])
            for kind, text in (("edit", edited), ("revert", original)):
                r = run_trial(w, acct, comp, path, text, args)
                r.update(iter=i, edit=e["name"], kind=kind, warmup=i < args.warmup)
                emit(r)
                if r["timed_out"]:
                    if kind == "edit":
                        write_file(path, original, args.atomic)
                    raise SessionAborted(f"{kind} {e['name']} (iter {i}) timed out")
                time.sleep(args.gap)
    except SessionAborted as e:
        print(f"  session aborted: {e}", file=sys.stderr, flush=True)
        emit({"kind": "session_abort", "reason": str(e)})
    finally:
        w.stop(acct)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config")
    ap.add_argument("--sessions", type=int, default=10)
    ap.add_argument("--iters", type=int, default=40)
    ap.add_argument("--warmup", type=int, default=5)
    ap.add_argument("--timeout", type=float, default=120)
    ap.add_argument(
        "--gap", type=float, default=0.2, help="idle seconds between trials"
    )
    ap.add_argument(
        "--atomic", action="store_true", help="write via temp file + rename"
    )
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument(
        "--mix",
        choices=["random", "stratified"],
        default="random",
        help="random: edits drawn independently; stratified: every edit once per pass",
    )
    ap.add_argument(
        "--accounting",
        choices=["auto", "direct", "systemd", "proc"],
        default="auto",
        help="direct: create cgroups yourself (root/delegated); systemd: "
        "systemd-run --user --scope; proc: /proc fallback",
    )
    ap.add_argument(
        "--cgroup-parent",
        help="parent dir for --accounting direct (default: cgroup2 mount root)",
    )
    ap.add_argument(
        "--perf",
        choices=["auto", "on", "off"],
        default="auto",
        help="hardware counters; on: fail if unavailable, auto: warn and continue",
    )
    ap.add_argument(
        "--perf-events",
        default="instructions,cycles",
        help=f"comma-separated, from: {', '.join(PERF_HW)}",
    )
    ap.add_argument(
        "--harness-cpus",
        help="pin the harness itself, e.g. 12 (keep it off the measured CPUs)",
    )
    ap.add_argument(
        "--idle-cores",
        type=float,
        default=0.02,
        help="CPU usage (in cores) below which the compiler counts as idle",
    )
    ap.add_argument("--out", default="results.jsonl")
    args = ap.parse_args()

    cfg = json.loads(Path(args.config).read_text())
    for c in cfg["compilers"]:
        c["_done_re"] = re.compile(c["done_regex"])
        c["_error_re"] = re.compile(c["error_regex"]) if c.get("error_regex") else None
        c["_stats_re"] = re.compile(c["stats_regex"]) if c.get("stats_regex") else None
        if c.get("trigger", "file") not in ("file", "stdin"):
            raise SystemExit(f"{c['name']}: trigger must be 'file' or 'stdin'")
        c["_stdin"] = c.get("trigger") == "stdin"

    if args.harness_cpus:
        cpus = {int(x) for x in args.harness_cpus.split(",")}
        os.sched_setaffinity(0, cpus)
        for c in cfg["compilers"]:
            if not c.get("cpus"):
                print(
                    f"WARNING: {c['name']} has no 'cpus' and inherits the harness CPUs",
                    file=sys.stderr,
                )

    perf_events, perf_error = [], None
    if args.perf != "off":
        perf_events = [e for e in args.perf_events.split(",") if e]
        unknown = [e for e in perf_events if e not in PERF_HW]
        if unknown:
            raise SystemExit(f"unknown --perf-events: {', '.join(unknown)}")
        perf_error = probe_perf(perf_events)
        if perf_error:
            if args.perf == "on":
                raise SystemExit(f"--perf on: {perf_error}")
            print(
                f"WARNING: hardware counters unavailable: {perf_error}", file=sys.stderr
            )
            perf_events = []

    mode, parent = choose_accounting(args.accounting, args.cgroup_parent)
    print(
        f"accounting: {mode}"
        + (f" under {parent}" if parent else "")
        + f"; perf: {','.join(perf_events) or 'off'}",
        flush=True,
    )
    counter, rng = [0], random.Random(args.seed)
    with open(args.out, "a") as out:
        out.write(
            json.dumps(meta_record(cfg, args, mode, parent, perf_events, perf_error))
            + "\n"
        )
        for sid in range(args.sessions):
            block = cfg["compilers"][:]
            rng.shuffle(block)  # randomized block design
            for comp in block:
                print(f"session {sid}: {comp['name']}", flush=True)
                run_session(
                    cfg, comp, sid, args, counter, out, mode, parent, perf_events
                )


if __name__ == "__main__":
    main()
