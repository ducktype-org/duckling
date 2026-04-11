use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::{DuckCtx, QuackResult};
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
        )
        .arg(flag(
            "external-errors",
            "Halt computation after encountering errors in foreign manifests",
        ))
}

/// Logic for executing the `build` subcommand.
pub fn execute(ctx: &DuckCtx, matches: &ArgMatches) -> QuackResult<()> {
    // We do not allow to build the global package.
    // It has no src folder and is purely for running scripts.
    let package = PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?;
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
