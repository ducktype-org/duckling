use crate::quackpack::subcommands::sync::SyncOptions;

#[derive(Clone, Copy, Debug)]
pub struct SolverMode {
    pub supress_foreign_manifests_errors: bool,
    pub offline: bool,
    pub frozen: bool,
}

impl From<SyncOptions> for SolverMode {
    fn from(value: SyncOptions) -> Self {
        Self {
            supress_foreign_manifests_errors: value.strict_errors,
            offline: value.offline,
            frozen: value.frozen,
        }
    }
}

#[cfg(test)]
impl Default for SolverMode {
    fn default() -> Self {
        Self {
            supress_foreign_manifests_errors: false,
            offline: false,
            frozen: false,
        }
    }
}
