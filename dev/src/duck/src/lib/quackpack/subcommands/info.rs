use crate::quackpack::core::PackageContext;
use crate::quackpack::core::storage::venv_id::ToVenvId;
use crate::quackpack::core::storage::{display_venv_info, venv_info};
use crate::util::error::{HintMessage, MessageError};
use crate::{QuackResult, QuackResultContext, qp_err};

/// Logic for executing the `info` subcommand.
pub fn info(pcx: PackageContext) -> QuackResult<()> {
    let ctx = pcx.ctx();
    let storage_localization = pcx.storage_path();
    let venv_id = pcx.to_venv_id();
    let Some((mut venv, previous_access)) = venv_info(storage_localization, venv_id, ctx)
        .context("when getting information about the venv")?
    else {
        let err = qp_err!(HintMessage::new(
            "make sure that the package is synchronized"
        ));
        let err = err.context(MessageError::new(format!(
            "did not find a venv for the package at `{}`",
            pcx.package().root().display()
        )));
        return Err(err);
    };
    // This info operation counts as access to the venv, modyfying the last_access to now.
    // To display a meaningful value of the last_access, we substitute the last_access field
    // with the last_access prior to this ongoing info operation.
    venv.data_mut().set_last_access(previous_access);
    display_venv_info(ctx, venv)
}
