//! Various options for controlling the packages' compilation.

use std::path::PathBuf;

use crate::StrId;

#[derive(Clone, Debug, Default)]
/// Options for controlling the build process.
pub struct BuildOptions {
    /// Link against the specified library.
    pub links: Option<StrId>,
    /// Shared objects loaded by the DVM, as written in the manifest.
    pub dvm_shared_libs: Vec<PathBuf>,
}
