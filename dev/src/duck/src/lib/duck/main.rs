use crate::QuackError;
use crate::duck::util::indent::indent;
use crate::util_common::error::QpErrorType;
use crate::{DuckCtx, duck::util::terminal::Terminal};
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
    let subscriber = EnvFilter::from_env("DUCK_DEBUG");
    let layer = layer()
        .with_timer(Uptime::default())
        .with_ansi(true)
        .with_filter(subscriber);

    registry().with(layer).init();
    debug!("start = {:#?}", std::time::SystemTime::now());
}

fn print_error_and_exit(error: QuackError, stdout: &Terminal, stderr: &Terminal) -> ! {
    if error.exit_code() == 0 {
        stdout.print(error);
        std::process::exit(0)
    } else {
        print_error(&error, stderr);
    }
    std::process::exit(error.exit_code())
}

fn print_error(error: &QuackError, term: &Terminal) {
    print_errors_stack(error, term);
    print_internals(error, term);
}

fn print_errors_stack(error: &QuackError, term: &Terminal) {
    for (i, e) in error.stack().enumerate() {
        if i == 0 {
            term.error(e);
        } else {
            term.print("");
            match e {
                QpErrorType::Hint(hint) => {
                    term.hint(indent(hint.as_ref().as_ref(), 2));
                }
                QpErrorType::Note(note) => {
                    term.note(indent(note.as_ref().as_ref(), 2));
                }
                QpErrorType::Error(e) => {
                    term.print(indent("Caused by:", 2));
                    term.print(indent(e.as_ref().as_ref(), 4));
                }
                QpErrorType::Internal(e) => {
                    term.print(indent("Caused by:", 2));
                    term.print(indent(e.as_ref().as_ref(), 4));
                }
            }
        }
    }
}

fn print_internals(error: &QuackError, term: &Terminal) {
    let mut internal_errors = false;
    for e in error.stack() {
        if let QpErrorType::Internal(e) = e {
            internal_errors = true;
            term.print("");
            term.critical(format!("got the internal error: {}", e.as_ref().as_ref()));
        }
    }
    if internal_errors {
        term.note("Please file a bug report at: https://github.com/ducktype-org/duckling/issues/");
    }
}
