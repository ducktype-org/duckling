use std::path::Path;

use crate::quackpack::core::PackageContext;
use crate::quackpack::core::storage::venv_id::ToVenvId;
use crate::quackpack::core::storage::{display_venv_info, venv_info};
use crate::{QuackResult, qp_bail, qp_err};

/// Display information about the current package's venv.
pub fn info(pcx: PackageContext) -> QuackResult<()> {
    let ctx = pcx.ctx();
    let storage_localization = pcx
        .venv_config()
        .storage_path()?
        .map(Path::to_path_buf)
        .unwrap_or_else(|| pcx.ctx().default_storage_root().into_not_locked_path());
    let venv_id = pcx.to_venv_id();
    let Some((mut venv, previous_access)) = venv_info(&storage_localization, venv_id, ctx)? else {
        let err = qp_err!("did not find a venv for the package");
        qp_bail!(err.add_hint(
            "make sure that the package is synchronized and the path to the storage is correct"
        ));
    };
    venv.data_mut().set_last_access(previous_access);
    display_venv_info(ctx, venv)
}
