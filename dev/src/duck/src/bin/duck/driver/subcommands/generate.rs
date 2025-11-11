use clap::{Arg, ArgAction, Command, crate_name, value_parser};
use clap_complete::{Generator, Shell, generate};
use quackpack::{InternalError, QuackResult};

use crate::DuckCtx;
use crate::driver::cli;
use crate::driver::cli_ext::subcommand;
use crate::terminal::Terminal;
use clap::ArgMatches;

pub fn get_parser() -> Command {
    subcommand("generate")
        .about("Generate shell completions")
        .arg(
            Arg::new("generator")
                .help("Choose target shell")
                .action(ArgAction::Set)
                .required(true)
                .value_parser(value_parser!(Shell)),
        )
}

pub fn execute(ctx: &DuckCtx, matches: &ArgMatches) -> QuackResult<()> {
    let Some(generator) = matches.get_one::<Shell>("generator").cloned() else {
        return Err(InternalError::from(
            "this should be guarded by a `.required(true)` in a parser",
        )
        .into());
    };
    print_completions(generator, cli(), ctx);
    Ok(())
}

fn print_completions<G: Generator>(generator: G, mut cli: Command, ctx: &DuckCtx) {
    let mut console: &Terminal = ctx.console();
    generate(generator, &mut cli, crate_name!(), &mut console);
}
