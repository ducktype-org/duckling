use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::{DuckContext, QuackResult};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{
    CommandExt, features_from_matches, flag, profile_from_matches, subcommand,
};

use crate::quackpack::subcommands::build::{BuildOptions, compile};

/// Creates parser for the `build` subcommand.
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
        .arg(flag("frozen", "Don't update the freezefile"))
        .arg(
            flag(
                "overwrite",
                "Overwrite any existing virtual environments with the same name",
            )
            .conflicts_with("global"),
        )
        .arg(
            flag(
                "global",
                "Build the package in the global virtual environment",
            )
            .conflicts_with("overwrite"),
        )
        .arg(flag(
            "external-errors",
            "Halt computation after encountering errors in foreign manifests",
        ))
}

/// Logic for executing the `build` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let global = matches.get_flag("global");
    let package = if global {
        PackageLoader::global_package(ctx)?
    } else {
        PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?
    };
    let features = features_from_matches(matches, package.package());
    let profile = profile_from_matches(matches);
    let opts = BuildOptions {
        package,
        used_features: features,
        profile,
        overwrite: matches.get_flag("overwrite"),
        frozen: matches.get_flag("frozen"),
        strict_errors: matches.get_flag("external-errors"),
    };
    compile(opts)?;
    Ok(())
}
