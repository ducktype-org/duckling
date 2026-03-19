//! Various (for us mostly unneeded) metadata of the root package.
use crate::StrId;

#[derive(Clone, Debug)]
/// Various package metadata.
/// This is mostly useless information for us, but it may be useful for a user.
pub struct PackageMetadata {
    authors: Vec<StrId>,
    license: Option<StrId>,
    description: Option<StrId>,
}

impl PackageMetadata {
    /// Create a new [`PackageMetadata`].
    pub fn new(authors: Vec<StrId>, license: Option<StrId>, description: Option<StrId>) -> Self {
        Self {
            authors,
            license,
            description,
        }
    }

    /// Get the package license
    pub fn license(&self) -> Option<StrId> {
        self.license
    }

    /// Get the package authors
    pub fn authors(&self) -> &[StrId] {
        &self.authors
    }

    /// Get the package description
    pub fn description(&self) -> Option<StrId> {
        self.description
    }

    pub fn into_authors(self) -> Vec<StrId> {
        self.authors
    }
}
