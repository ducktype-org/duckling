use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{Arg, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, subcommand};

pub fn get_parser() -> Command {
    subcommand("tree")
        .about("Print the dependency tree of the package")
        .arg(flag(
            "no-dedup",
            "Don't deduplicate the subtrees of the same package",
        ))
        .arg(
            Arg::new("max-depth")
                .help("Set the maximal displayed depth of the tree")
                .value_parser(1..),
        )
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement tree")
}
