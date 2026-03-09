use crate::quackpack::subcommands::sync::SyncOptions;

#[derive(Debug, Clone, Copy)]
#[cfg_attr(test, derive(Default))]
pub struct SolverMode {
    pub supress_foreign_manifests_errors: bool,
    pub frozen: bool,
}

impl From<SyncOptions> for SolverMode {
    fn from(value: SyncOptions) -> Self {
        Self {
            supress_foreign_manifests_errors: value.strict_errors,
            frozen: value.frozen,
        }
    }
}
