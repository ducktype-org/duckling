use std::{fs::File, io::Write};

use crate::{DuckCtx, QuackResult, StrId, quackpack::core::PackageLoader};

pub struct ActivateOptions<'duck> {
    /// Current [`DuckCtx`].
    pub ctx: &'duck DuckCtx,
    /// Optional name of the venv to run the script in.
    pub venv_id: Option<StrId>,
    /// Activate the global venv
    pub global: bool,
}

/// Run script given options.
pub fn activate<'duck>(options: ActivateOptions<'duck>) -> QuackResult<()> {
    // Assure the venv exists
    match options.venv_id {
        Some(venv_id) => {
            debug_assert!(!options.global);
            PackageLoader::find_venv_by_name(options.ctx, venv_id)?;
        }
        None => {
            debug_assert!(options.global);
        }
    };
    let venv_id = options.venv_id.unwrap_or("global".into());
    let mut active_venv_file = File::create(options.ctx.duck_home().active_venv_file())?;
    write!(active_venv_file, "{}", venv_id)?;
    Ok(())
}
