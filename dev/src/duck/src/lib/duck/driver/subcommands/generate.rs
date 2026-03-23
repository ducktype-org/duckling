use clap::{Arg, ArgAction, Command, crate_name, value_parser};
use clap_complete::{Generator, Shell, generate};

use crate::{
    DuckCtx, QuackResult,
    duck::{
        driver::{cli, cli_ext::subcommand},
        util::terminal::Terminal,
    },
    qp_bail_internal,
};
use clap::ArgMatches;

/// Creates parser for the `generate` subcommand.
pub fn get_parser() -> Command {
    subcommand("generate")
        .about("Generate shell completions")
        .arg(
            Arg::new("generator")
                .help("Choose the target shell")
                .action(ArgAction::Set)
                .required(true)
                .value_parser(value_parser!(Shell)),
        )
}

/// Logic for executing the `generate` subcommand.
pub fn execute(ctx: &DuckCtx, matches: &ArgMatches) -> QuackResult<()> {
    let Some(generator) = matches.get_one::<Shell>("generator").cloned() else {
        qp_bail_internal!("this should be guarded by a `.required(true)` in a parser")
    };
    print_completions(generator, cli(), ctx);
    Ok(())
}

/// Print shell completion file to the stdout.
fn print_completions<G: Generator>(generator: G, mut cli: Command, ctx: &DuckCtx) {
    let mut console: &Terminal = ctx.console();
    generate(generator, &mut cli, crate_name!(), &mut console);
}
