use crate::{
    DuckCtx, QuackResult, QuackResultContext, StrId,
    quackpack::{
        core::{
            FeatureName, PackageCtx,
            compile::{self, BuildContext},
            storage::{sync, venv_id::ToVenvId},
        },
        subcommands::sync::SyncOptions,
    },
};

#[derive(Debug)]
pub struct BuildOptions<'duck> {
    pub ctx: &'duck DuckCtx,
    pub package: PackageCtx<'duck>,
    pub used_features: Vec<FeatureName>,
    pub profile: StrId,
    pub global: bool,
    pub overwrite: bool,
    pub frozen: bool,
    pub strict_errors: bool,
}

pub fn compile<'duck>(options: BuildOptions<'duck>) -> QuackResult<()> {
    let BuildOptions {
        ctx,
        package,
        used_features,
        profile,
        global,
        overwrite,
        frozen,
        strict_errors,
    } = options;
    let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .unwrap();
    let (lock, venv, storage) = rt.block_on(async {
        sync(
            ctx,
            &package,
            SyncOptions {
                global,
                overwrite,
                frozen,
                strict_errors,
            },
        )
    })?;
    let _compile_lock = lock
        .to_compile_lock(&storage, package.to_venv_id())
        .context("failed to acquire a compile lock")?;
    let bctx = BuildContext {
        duck_ctx: ctx,
        package: &package,
        freeze: venv.into(),
        storage,
        used_features,
        profile,
    };
    compile::compile(bctx)?;
    Ok(())
}
