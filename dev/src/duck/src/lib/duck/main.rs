use crate::duck::util::indent::indent;
use crate::util::error::{DisplayPlace, ErrorExt, ErrorType};
use crate::{DuckCtx, duck::util::terminal::Terminal};
use crate::{QuackError, QuackResult, qp_bail_internal};
use tracing::debug;

/// Actual main entry point for the duck-binary.
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

/// Setup [`tracing`] loggers.
pub fn setup_logger() {
    use tracing_subscriber::{
        EnvFilter, Layer,
        fmt::{layer, time::Uptime},
        prelude::*,
        registry,
    };
    let subscriber = EnvFilter::from_env("DUCK_DEBUG");
    let layer = layer()
        .with_timer(Uptime::default())
        .with_ansi(true)
        .with_filter(subscriber);

    registry().with(layer).init();
    debug!("start = {:#?}", std::time::SystemTime::now());
}

/// Print returned [`QuackError`] to the appropriate [`Terminal`] and exit.
fn print_error_and_exit(error: QuackError, stdout: &Terminal, stderr: &Terminal) -> ! {
    if matches!(error.display_place(), DisplayPlace::StdOut) {
        if let Err(e) = print_message(&error, stdout) {
            print_error_and_exit(e, stdout, stderr)
        }
    } else {
        print_error(&error, stderr);
    }
    std::process::exit(error.exit_code())
}

/// Print [`QuackError`] as a message.
fn print_message(msgs: &QuackError, term: &Terminal) -> QuackResult<()> {
    for (i, error) in msgs.sources().enumerate() {
        if i > 0 {
            term.print("");
        }
        let msg = if let Some(clap_error) = error.context_aware_downcast_ref::<clap::Error>() {
            // Clap internally adds a trailing newline, remove it.
            clap_error
                .render()
                .ansi()
                .to_string()
                .trim_end()
                .to_string()
        } else {
            error.to_string()
        };
        match error.error_type() {
            ErrorType::Hint => {
                term.hint(msg);
            }
            ErrorType::Note => {
                term.note(msg);
            }
            ErrorType::BareMessage => {
                term.print(msg);
            }
            _ => qp_bail_internal!("Errors and internal errors should not be printed on stdout"),
        }
    }
    Ok(())
}

/// Print [`QuackError`] as an error.
fn print_error(error: &QuackError, term: &Terminal) {
    print_errors_stack(error, term);
    print_internals(error, term);
}

/// Print stack of [`QuackError`]s.
fn print_errors_stack(error: &QuackError, term: &Terminal) {
    for (i, error) in error.sources().enumerate() {
        let (msg, is_clap) =
            if let Some(clap_error) = error.context_aware_downcast_ref::<clap::Error>() {
                (
                    // Clap internally adds a trailing newline, remove it.
                    clap_error
                        .render()
                        .ansi()
                        .to_string()
                        .trim_end()
                        .to_string(),
                    true,
                )
            } else {
                (error.to_string(), false)
            };
        if i == 0 {
            // Clap errors already start with `error: ` prefix, ignore it.
            if is_clap {
                term.print(msg);
            } else {
                term.error(msg);
            }
        } else {
            term.print("");
            match error.error_type() {
                ErrorType::Hint => {
                    term.hint(msg);
                }
                ErrorType::Note => {
                    term.note(msg);
                }
                ErrorType::BareMessage => {
                    term.print(msg);
                }
                ErrorType::Error => {
                    term.print(indent("Caused by:", 2));
                    term.print(indent(msg.as_str(), 4));
                }
                ErrorType::Internal => {
                    term.print(indent("Caused by:", 2));
                    term.print(indent(msg.as_str(), 4));
                }
            }
        }
    }
}

/// Print all internal errors in the [`QuackError`] stack.
///
/// This is done at the end, so URL shows at the bottom of the user's terminal.
fn print_internals(error: &QuackError, term: &Terminal) {
    let mut internal_errors = false;
    for e in error.sources() {
        if e.error_type() == ErrorType::Internal {
            internal_errors = true;
            term.print("");
            term.critical(format!("got the internal error: {}", e));
        }
    }
    if internal_errors {
        term.note("Please file a bug report at: https://github.com/ducktype-org/duckling/issues/");
    }
}
