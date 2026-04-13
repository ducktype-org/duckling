//! Querying a storage's data.
use std::collections::HashMap;
use std::path::Path;

use storage::paths;

use crate::QuackResult;
use crate::quackpack::core::storage;
use crate::quackpack::core::storage::venv::Venv;
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};

/// Get a snapshot of all virtual environments' states.
///
/// The combined state may never have existed in storage as a consistent whole; this function locks each
/// virtual environment separately. Equivalent to calling [`venv_info`] on all virtual environments present
/// in the storage.
pub fn list_venvs(storage_root: &Path) -> QuackResult<HashMap<VenvId, Venv>> {
    let storage = paths::Storage::new(storage_root);
    let mut metadata = HashMap::new();
    let vevns = storage.iter_venvs()?.collect::<Result<Vec<_>, _>>()?;
    for venv in vevns {
        if !venv.path().is_dir() {
            continue;
        }
        let id = venv.file_name().to_venv_id();
        let data = Venv::fix_and_load(&storage, id)?;
        if let Some(data) = data {
            metadata.insert(id, data);
        }
    }
    Ok(metadata)
}

/// Retrieve the storage state of a specific virtual environment.
pub fn venv_info(storage_root: &Path, id: impl ToVenvId) -> QuackResult<Option<Venv>> {
    let storage = paths::Storage::new(storage_root);
    let id = id.to_venv_id();
    let data = Venv::fix_and_load(&storage, id)?;
    Ok(data)
}
