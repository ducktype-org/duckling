use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{
    CommandExt, features_from_matches, flag, jobs_from_matches, multi, profile_from_matches,
    subcommand,
};
use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::quackpack::subcommands::build::{BuildOptions, compile};
use crate::{DuckContext, QuackResult};

/// Creates parser for the `build` subcommand.
pub fn get_parser() -> Command {
    subcommand("build")
        .about("Build the current package")
        .add_profile()
        .add_release()
        .arg(
            multi("features", "Build the current package with these features")
                .short('F')
                .conflicts_with("all-features"),
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
        .arg(flag(
            "overwrite",
            "Overwrite any existing virtual environments with the same name",
        ))
        .arg(flag(
            "external-errors",
            "Halt computation after encountering errors in foreign manifests",
        ))
        .arg(flag(
            "shared-artifacts",
            "Compile dependencies where their code is located",
        ))
}

/// Logic for executing the `build` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    // We do not allow to build the global package.
    // It has no src folder and is purely for running scripts.
    let pcx = PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?;
    let features = features_from_matches(matches, pcx.package().get_package());
    let profile = profile_from_matches(matches);
    let opts = BuildOptions {
        pcx,
        used_features: features,
        profile,
        shared: matches.get_flag("shared-artifacts"),
        overwrite: matches.get_flag("overwrite"),
        frozen: matches.get_flag("frozen"),
        strict_errors: matches.get_flag("external-errors"),
        jobs: jobs_from_matches(matches),
    };
    compile(opts)?;
    Ok(())
}
