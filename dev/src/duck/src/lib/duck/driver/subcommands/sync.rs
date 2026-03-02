use crate::{
    DuckCtx, QpCtx, QuackResult,
    quackpack::core::{
        AllowGlobalPackage, PackageLoader,
        storage::{SyncOptions, sync},
    },
};
use clap::{ArgMatches, Command};
use tokio::runtime;

use crate::duck::driver::cli_ext::{flag, subcommand};

pub fn get_parser() -> Command {
    subcommand("sync")
        .about("Synchronize the current venv")
        .arg(flag("frozen", "Don't update the freezefile"))
        .arg(flag("offline", "Don't perform any network requests"))
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
        .arg(flag("external_errors", "Halt computation after encountering errors in foreign manifests"))
}

pub fn execute(ctx: &DuckCtx, matches: &ArgMatches) -> QuackResult<()> {
    let qp_ctx = QpCtx::new(ctx);
    let pkg = if matches.get_flag("global") {
        PackageLoader::global_package(&qp_ctx)?
    } else {
        PackageLoader::find_from_cwd(&qp_ctx, AllowGlobalPackage::No)?
    };
    let rt = runtime::Builder::new_multi_thread().enable_all().build()?;
    rt.block_on(async {
        sync(
            ctx,
            &pkg,
            SyncOptions {
                overwrite: matches.get_flag("overwrite"),
                frozen: matches.get_flag("frozen"),
                offline: matches.get_flag("offline"),
                strict_errors: matches.get_flag("external_errors"),
            },
        )
    })?;
    Ok(())
}
