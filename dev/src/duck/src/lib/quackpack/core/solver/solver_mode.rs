use crate::quackpack::core::storage::StorageSyncOptions;

#[derive(Debug, Clone, Copy)]
#[cfg_attr(test, derive(Default))]
/// A struct containing options for the solver.
/// The options are as follows:
/// * suppress_foreign_manifests_errors - whether errors in foreign manifests
///   (such as requiring a non-existent feature) should be propagated and end quackpack's
///   execution with an error or be suppressed,
/// * frozen - whether to return an error if the previous freeze is no longer valid.
pub struct SolverMode {
    pub suppress_foreign_manifests_errors: bool,
    pub frozen: bool,
}

impl From<StorageSyncOptions> for SolverMode {
    fn from(value: StorageSyncOptions) -> Self {
        Self {
            suppress_foreign_manifests_errors: value.strict_errors,
            frozen: value.frozen,
        }
    }
}
