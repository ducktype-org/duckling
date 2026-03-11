use std::collections::HashMap;

use crate::quackpack::core::storage;

use crate::quackpack::core::storage::venv::Venv;
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
use crate::util_common::path_ops_ext::{PathOpsExt, ShouldBlock};
use crate::{DuckCtx, QuackResult};
use storage::paths;

/// Get a snapshot of all virtual environments' states.
///
/// The combined state may never have existed in storage as a consistent whole; this function locks each
/// virtual environment separately. Equivalent to calling [`venv_info`] on all virtual environments present
/// in the storage.
pub fn list_venvs(ctx: &DuckCtx) -> QuackResult<HashMap<VenvId, Venv>> {
    let storage = paths::Storage::new(ctx.default_storage_root().to_path_buf());
    let mut metadata = HashMap::new();
    let vevns = storage.iter_venvs()?.collect::<Result<Vec<_>, _>>()?;
    for venv in vevns {
        if !venv.path().is_dir() {
            continue;
        }
        let id = venv.file_name().into();
        let _lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
        let data = Venv::fix_and_load(&storage, id)?;
        if let Some(data) = data {
            metadata.insert(id, data);
        }
    }
    Ok(metadata)
}

/// Retrieve the storage state of a specific virtual environment.
pub fn venv_info(ctx: &DuckCtx, id: impl ToVenvId) -> QuackResult<Option<Venv>> {
    let storage = paths::Storage::new(ctx.default_storage_root().to_path_buf());
    let id = id.to_venv_id();
    let _lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
    let data = Venv::fix_and_load(&storage, id)?;
    Ok(data)
}
