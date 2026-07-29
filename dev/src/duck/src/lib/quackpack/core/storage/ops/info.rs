//! Querying a storage's data.
use std::collections::HashMap;
use std::path::Path;
use std::time::SystemTime;

use chrono::DateTime;
use storage::paths;

use crate::quackpack::core::storage;
use crate::quackpack::core::storage::venv::Venv;
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
use crate::{DuckContext, QuackResult};

/// Get a snapshot of all virtual environments' states.
///
/// The combined state may never have existed in storage as a consistent whole; this function locks each
/// virtual environment separately. Equivalent to calling [`venv_info`] on all virtual environments present
/// in the storage.
pub fn list_venvs(
    storage_root: &Path,
    ctx: &DuckContext,
) -> QuackResult<HashMap<VenvId, (Venv, SystemTime)>> {
    let storage = paths::Storage::new(storage_root);
    let mut metadata = HashMap::new();
    let venvs = storage.iter_venvs()?.collect::<Result<Vec<_>, _>>()?;
    for venv in venvs {
        if !venv.path().is_dir() {
            continue;
        }
        let id = venv.file_name().to_venv_id();
        let data = Venv::fix_and_load_with_last_access(&storage, id, ctx)?;
        if let Some(data) = data {
            metadata.insert(id, data);
        }
    }
    Ok(metadata)
}

/// Retrieve the storage state of a specific virtual environment.
pub fn venv_info(
    storage_root: &Path,
    id: impl ToVenvId,
    ctx: &DuckContext,
) -> QuackResult<Option<(Venv, SystemTime)>> {
    let storage = paths::Storage::new(storage_root);
    let id = id.to_venv_id();
    let data = Venv::fix_and_load_with_last_access(&storage, id, ctx)?;
    Ok(data)
}

/// Nicely displays the information about the venv to the user.
pub fn display_venv_info(ctx: &DuckContext, venv: Venv) -> QuackResult<()> {
    let last_access_date: DateTime<chrono::Local> = venv.data().last_access().into();
    let last_access_string = last_access_date.format("%Y-%m-%d %H:%M:%S").to_string();
    let last_synchronization_date: DateTime<chrono::Local> = venv.data().last_synchronization().into();
    let last_synchronization_string = last_synchronization_date
        .format("%Y-%m-%d %H:%M:%S")
        .to_string();
    ctx.console().print(format!(
        "{}:
  last-location: {}
  last-access: {}
  last-modification: {}
  {}ephemeral
",
        venv.id(),
        venv.data().last_known_location().display(),
        last_access_string,
        last_synchronization_string,
        if venv.data().is_ephemeral() {
            ""
        } else {
            "not "
        },
    ))
}
