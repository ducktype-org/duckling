use clap::{Arg, ArgAction, ArgMatches, Command, crate_name, value_parser};
use clap_complete::{Generator, Shell, generate};

use crate::duck::driver::cli;
use crate::duck::driver::cli_ext::subcommand;
use crate::duck::util::terminal::Terminal;
use crate::{DuckContext, QuackResult, qp_bail_internal};

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
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let Some(generator) = matches.get_one::<Shell>("generator").cloned() else {
        qp_bail_internal!("this should be guarded by a `.required(true)` in a parser")
    };
    print_completions(generator, cli(), ctx);
    Ok(())
}

/// Print shell completion file to the stdout.
fn print_completions<G: Generator>(generator: G, mut cli: Command, ctx: &DuckContext) {
    let mut console: &Terminal = ctx.console();
    generate(generator, &mut cli, crate_name!(), &mut console);
}
