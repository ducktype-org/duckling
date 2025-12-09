use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{CommandExt, flag, subcommand};

pub fn get_parser() -> Command {
    subcommand("build")
        .about("Build the current package")
        .add_profile()
        .add_release()
        .add_features_conflicting(
            "Build the current package with these features",
            "all-features",
        )
        .arg(
            flag(
                "all-features",
                "Build the current package with all possible features",
            )
            .conflicts_with("features"),
        )
        .add_jobs()
}
pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement build")
}
