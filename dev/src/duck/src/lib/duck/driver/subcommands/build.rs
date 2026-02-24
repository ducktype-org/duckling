use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::{DuckCtx, QpCtx, QuackResult, qp_bail};
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
}
pub fn execute(ctx: &DuckCtx, matches: &ArgMatches) -> QuackResult<()> {
    let qpctx = QpCtx::new(ctx);
    let package = PackageLoader::find_from_cwd(&qpctx, AllowGlobalPackage::No)?;
    let features = features_from_matches(matches, package.package());
    let profile = profile_from_matches(matches);
    let opts = BuildOptions {
        ctx,
        package,
        used_features: features,
        profile,
    };
    compile(opts)?;
    Ok(())
}
