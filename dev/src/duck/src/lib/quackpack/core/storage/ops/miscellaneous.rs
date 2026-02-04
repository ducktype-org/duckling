use std::collections::HashMap;

use rustvil::fs::{PathExt, ShouldBlock};

use crate::quackpack::core::storage;

use crate::StrId;
use crate::{DuckCtx, QuackResult};
use storage::IdOrPackage;
use storage::files::StorageVenv;
use storage::files::fix_and_load_venv;
use storage::paths;

/// Get a snapshot of all virtual environments' states.
///
/// The combined state may never have existed in storage as a consistent whole; this function locks each
/// virtual environment separately. Equivalent to calling [`venv_info`] on all virtual environments present
/// in the storage.
pub fn list_venvs(ctx: &DuckCtx) -> QuackResult<HashMap<StrId, StorageVenv>> {
    let storage = paths::StoragePaths::new(ctx.duck_home());
    let mut metadata = HashMap::new();
    let vevns = storage.iter_vens()?.collect::<Result<Vec<_>, _>>()?;
    for venv in vevns {
        if !venv.path().is_dir() {
            continue;
        }
        let id = venv.file_name().to_string_lossy().into_owned().into();
        let _lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
        let data = fix_and_load_venv(&storage, id)?;
        if let Some(data) = data {
            metadata.insert(id, data);
        }
    }
    Ok(metadata)
}

/// Retrieve the storage state of a specific virtual environment.
pub fn venv_info(ctx: &DuckCtx, id: IdOrPackage<'_>) -> QuackResult<Option<StorageVenv>> {
    let storage = paths::StoragePaths::new(ctx.duck_home());
    let id = id.venv_id();
    let _lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
    let data = fix_and_load_venv(&storage, id)?;
    Ok(data)
}
