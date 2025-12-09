use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{CommandExt, subcommand};

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

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement add")
}
