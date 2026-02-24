use crate::{
    DuckCtx, QuackResult, QuackResultContext, StrId,
    quackpack::core::{
        FeatureName, PackageCtx,
        compile::{self, BuildContext},
        storage::{SyncOptions, sync, venv_id::ToVenvId},
    },
};

#[derive(Debug)]
pub struct BuildOptions<'duck> {
    pub ctx: &'duck DuckCtx,
    pub package: PackageCtx<'duck>,
    pub used_features: Vec<FeatureName>,
    pub profile: StrId,
}

pub fn compile<'duck>(options: BuildOptions<'duck>) -> QuackResult<()> {
    let BuildOptions {
        ctx,
        package,
        used_features,
        profile,
    } = options;
    let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .unwrap();
    let (lock, venv, storage) =
        rt.block_on(async { sync(ctx, &package, SyncOptions::default()) })?;
    let _compile_lock = lock
        .to_compile_lock(&storage, package.to_venv_id())
        .context("failed to acquire compile lock")?;
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
