use tokio::runtime;

use crate::{
    DuckCtx, QpCtx, QuackResult,
    quackpack::core::{AllowGlobalPackage, PackageLoader, storage},
};

#[derive(Debug, Default, Clone, Copy)]
pub struct SyncOptions {
    pub global: bool,
    pub overwrite: bool,
    pub frozen: bool,
    pub strict_errors: bool,
}

pub fn sync(ctx: &DuckCtx, options: SyncOptions) -> QuackResult<()> {
    let qp_ctx = QpCtx::new(ctx);
    let pkg = if options.global {
        PackageLoader::global_package(&qp_ctx)?
    } else {
        PackageLoader::find_from_cwd(&qp_ctx, AllowGlobalPackage::No)?
    };
    let rt = runtime::Builder::new_multi_thread().enable_all().build()?;
    rt.block_on(async { storage::sync(ctx, &pkg, options) })?;
    Ok(())
}
