__all__ = ["main"]


def main() -> None:
    from quackpack.signals import SignalInterrupt

    try:
        from signal import SIGINT, SIGTERM, signal

        from quackpack.signals import raising_signal_handler

        signal(SIGINT, raising_signal_handler)
        signal(SIGTERM, raising_signal_handler)
        from quackpack.cli.setup_and_run import setup_and_run

        setup_and_run()
    except SignalInterrupt as e:
        import sys

        # C standard doesn't specify anything about return code of uncaught signals.
        # Newest version of POSIX currently specifies, that return code should be greater than 128.
        # https://pubs.opengroup.org/onlinepubs/9799919799/utilities/V3_chap02.html#tag_19_08_02
        # Older POSIX mentioned, that some shells, for historical reasons, performed `exit()`
        # with return code = `128 + n`, where `n` is `(int)signum`.
        # https://pubs.opengroup.org/onlinepubs/9699919799/xrat/V4_xcu_chap02.html#tag_23_02_08
        sys.exit(128 + e.signum)
