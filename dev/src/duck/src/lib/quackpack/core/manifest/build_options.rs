//! Various options for controlling the packages' compilation.

#[derive(Clone, Debug, Default)]
/// Options for controlling the build process.
pub struct BuildOptions {
    /// Link against the specified library.
    pub links: Option<String>,
}
