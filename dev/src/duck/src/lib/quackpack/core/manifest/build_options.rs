//! Various options for controlling the packages' compilation.

use crate::StrId;

#[derive(Clone, Debug, Default)]
/// Options for controlling the build process.
pub struct BuildOptions {
    /// Link against the specified library.
    pub links: Option<StrId>,
    /// Shared objects the DVM `dlopen`s at runtime.
    pub dvm_shared_libs: Vec<StrId>,
}
