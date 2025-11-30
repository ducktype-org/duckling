use crate::duck::util::indent::indent;
use crate::{DuckCtx, InternalError, duck::util::terminal::Terminal};
use tracing::debug;

pub fn main() {
    setup_logger();
    let mut ctx = match DuckCtx::new() {
        Ok(ctx) => ctx,
        Err(err) => {
            let stdout = Terminal::stdout();
            let stderr = Terminal::stderr();
            print_error_and_exit(err, &stdout, &stderr);
        }
    };
    if let Err(e) = crate::duck::driver::run::run(&mut ctx) {
        print_error_and_exit(e, ctx.console(), ctx.error_console())
    }
}

fn setup_logger() {
    use tracing_subscriber::{
        EnvFilter, Layer,
        fmt::{layer, time::Uptime},
        prelude::*,
        registry,
    };
    // @TODO: #1353 Something like `DUCK_DEBUG`? On the other hand it also affects loggers in the quackpack (the library),
    //        but the cli tool is called duck...
    let subscriber = EnvFilter::from_env("QP_DEBUG");
    let layer = layer()
        .with_timer(Uptime::default())
        .with_ansi(true)
        .with_filter(subscriber);

    registry().with(layer).init();
    debug!("start = {:#?}", std::time::SystemTime::now());
}

fn print_error_and_exit(error: anyhow::Error, stdout: &Terminal, stderr: &Terminal) -> ! {
    if let Some(clap_err) = error.downcast_ref::<clap::Error>() {
        let error_msg = clap_err.render();
        let term = if clap_err.use_stderr() {
            stderr
        } else {
            stdout
        };
        term.print_no_nl(error_msg.ansi());

        let code = if matches!(clap_err.kind(), clap::error::ErrorKind::DisplayHelp) {
            0
        } else {
            1
        };
        std::process::exit(code)
    }
    print_error(error, stderr);
    let code = 1;
    std::process::exit(code)
}

fn print_error(error: anyhow::Error, term: &Terminal) {
    for (i, e) in error.chain().enumerate() {
        if i == 0 {
            term.error(e);
        } else {
            term.print("");
            term.print(indent("Caused by:", 2));
            term.print(indent(&e.to_string(), 4));
        }
    }

    // NOTE: This is tricky with contexts. Effectively they get some special type so even doing
    // `.with_context(|| InternalError::from(...))` won't show them here.
    // I see two solutions:
    //  1. (current): get the highest `InternalError` (remember that the original error is at the bottom of the stack,
    //     and at the top is the last context). This works even with `InternalError` in the context.
    //  2. use `.downcast_ref::<InternalError>()` with `.flat_map()` to get all the `InternalError`s.
    //     This is tricky, because contexts get some weird type and can't be downcasted, therefore this doesn't
    //     catch the contexts.
    //
    // Tested on the following snippet:
    // ```rust
    // let x: QuackResult<()> = Err(InternalError::from(anyhow!("error")).into());
    // x.context("b").context("a")?;
    // ```
    // With some playing with an error and context types to see what gets printed.
    //
    // Note that both options show at most one `InternalError`, but first shows one always,
    // whereas second only if the `InternalError` is at the bottom of the stack.
    //
    // Docs: https://docs.rs/anyhow/latest/anyhow/trait.Context.html#effect-on-downcasting
    if let Some(e) = error.downcast_ref::<InternalError>() {
        // Add a newline between backtrace and a critical error.
        term.print("");
        term.critical(format!("got the internal error: {e}"));
        term.note("Please file a bug report at: https://github.com/ducktype-org/duckling/issues/");
    }

    // Second approach.
    #[cfg(false)]
    {
        let mut has_internal_errors = false;
        for (i, e) in error
            .chain()
            .flat_map(|e| e.downcast_ref::<InternalError>())
            .enumerate()
        {
            has_internal_errors = true;
            // Add a newline between backtrace and a critical errors.
            if i == 0 {
                term.print("");
            }
            term.critical(format!("got the internal error: {e}"));
        }
        if has_internal_errors {
            term.note(
                "Please file a bug report at: https://github.com/ducktype-org/duckling/issues/",
            );
        }
    }
}
