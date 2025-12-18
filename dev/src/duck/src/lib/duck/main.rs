use crate::duck::util::indent::indent;
use crate::util_common::error::{DisplayPlace, QpErrorType};
use crate::{DuckCtx, duck::util::terminal::Terminal};
use crate::{QuackError, QuackResult, qp_bail_internal};
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
    if matches!(error.display_place(), DisplayPlace::StdOut) {
        if let Err(e) = print_message(&error, stdout) {
            print_error_and_exit(e, stdout, stderr)
        }
    } else {
        print_error(&error, stderr);
    }
    std::process::exit(error.exit_code())
}

fn print_message(msgs: &QuackError, term: &Terminal) -> QuackResult<()> {
    for (i, msg) in msgs.stack().enumerate() {
        if i > 0 {
            term.print("");
        }
        match msg {
            QpErrorType::Hint(hint) => {
                term.hint(hint.as_ref().as_ref());
            }
            QpErrorType::Note(note) => {
                term.note(note.as_ref().as_ref());
            }
            QpErrorType::BareMessage(msg) => {
                term.print(msg.as_ref().as_ref());
            }
            _ => qp_bail_internal!("Errors and internal errors should not be printed on stdout"),
        }
    }
    Ok(())
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
                    term.hint(hint.as_ref().as_ref());
                }
                QpErrorType::Note(note) => {
                    term.note(note.as_ref().as_ref());
                }
                QpErrorType::BareMessage(msg) => {
                    term.print(msg.as_ref().as_ref());
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
