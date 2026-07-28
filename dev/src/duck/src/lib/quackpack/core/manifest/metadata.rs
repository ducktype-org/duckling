//! Various (for us mostly unneeded) metadata of the root package.

#[derive(Clone, Debug, Default)]
/// Various package metadata.
/// This is mostly useless information for us, but it may be useful for a user.
pub struct PackageMetadata {
    pub authors: Vec<String>,
    pub license: Option<String>,
    pub description: Option<String>,
}
