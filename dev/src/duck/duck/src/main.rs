use crate::terminal::Terminal;

pub mod driver;
pub mod duck_cfg;
pub mod duck_ctx;
mod env;
mod terminal;

pub use duck_ctx::DuckCtx;
use tracing::debug;

fn main() {
    setup_logger();
    let mut ctx = match DuckCtx::new() {
        Ok(ctx) => ctx,
        Err(err) => {
            let term = Terminal::stderr();
            print_error_and_exit(err, &term)
        }
    };
    if let Err(e) = driver::run::run(&mut ctx) {
        print_error_and_exit(e, ctx.error_console())
    }
}

fn setup_logger() {
    use tracing_subscriber::{
        EnvFilter, Layer,
        fmt::{layer, time::Uptime},
        prelude::*,
        registry,
    };
    let subscriper = EnvFilter::from_env("QP_DEBUG");
    let layer = layer()
        .with_timer(Uptime::default())
        .with_ansi(true)
        .with_filter(subscriper);

    let registry = registry().with(layer);
    registry.init();
    debug!("start = {:#?}", std::time::SystemTime::now());
}

fn print_error_and_exit(error: anyhow::Error, term: &Terminal) -> ! {
    if let Some(clap_err) = error.downcast_ref::<clap::Error>() {
        let _ = clap_err.print();
        let code = 1;
        std::process::exit(code)
    }
    print_error(error, term);
    let code = 1;
    std::process::exit(code)
}

fn print_error(error: anyhow::Error, term: &Terminal) {
    for (i, e) in error.chain().enumerate() {
        if i == 0 {
            term.error(e);
        } else {
            term.print("");
            term.print(format!("{:indent$}Caused by:", "", indent = 2));
            term.print(format!("{:indent$}{}", "", e, indent = 4));
        }
    }
}
