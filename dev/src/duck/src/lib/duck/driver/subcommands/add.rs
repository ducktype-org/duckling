use crate::{DuckContext, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{CommandExt, subcommand};

/// Creates parser for the `add` subcommand.
pub fn get_parser() -> Command {
    subcommand("add")
        .about("Add packages to the current venv")
        .add_dev("Add packages as dev dependencies")
        .add_global_venv()
        .add_local_git_deps(
            "Add dependencies as local dependencies",
            "Add dependencies as git dependencies",
        )
        .add_features("Enable features for new packages")
        .add_packages("Packages to add")
}

/// Logic for executing the `add` subcommand.
pub fn execute(_ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement add")
}
