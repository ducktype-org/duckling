use crate::{
    DuckCtx, QuackResult,
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
    let pkg = if options.global {
        PackageLoader::global_package(ctx)?
    } else {
        PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?
    };
    storage::sync(&pkg, options)?;
    Ok(())
}
