use anyhow::bail;
use clap::{Arg, ArgMatches, Command};
use duck_lib::QuackResult;

use crate::{
    DuckCtx,
    driver::cli_ext::{flag, subcommand},
};

pub fn get_parser() -> Command {
    subcommand("tree")
        .about("Print the dependency tree of a package")
        .arg(flag("no-dedup", "Show subtree of a package everytime"))
        .arg(
            Arg::new("max-depth")
                .help("Set the maximal displayed depth of the tree")
                .value_parser(1..),
        )
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement tree")
}
