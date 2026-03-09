use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::{DuckCtx, QpCtx, QuackResult};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{
    CommandExt, features_from_matches, flag, profile_from_matches, subcommand,
};

use crate::quackpack::subcommands::build::{BuildOptions, compile};

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
            flag("global", "Synchronize the global virtual environment")
                .conflicts_with("overwrite"),
        )
        .arg(flag(
            "external-errors",
            "Halt computation after encountering errors in foreign manifests",
        ))
}
pub fn execute(ctx: &DuckCtx, matches: &ArgMatches) -> QuackResult<()> {
    let qpctx = QpCtx::new(ctx);
    let global = matches.get_flag("global");
    let package = if global {
        PackageLoader::global_package(&qpctx)?
    } else {
        PackageLoader::find_from_cwd(&qpctx, AllowGlobalPackage::No)?
    };
    let features = features_from_matches(matches, package.package());
    let profile = profile_from_matches(matches);
    let opts = BuildOptions {
        ctx,
        package,
        used_features: features,
        profile,
        global,
        overwrite: matches.get_flag("overwrite"),
        frozen: matches.get_flag("frozen"),
        strict_errors: matches.get_flag("external-errors"),
    };
    compile(opts)?;
    Ok(())
}
