//! Various (for us mostly unneeded) metadata of the root package.

#[derive(Clone, Debug)]
/// Various package metadata.
/// This is mostly useless information for us, but it may be useful for a user.
pub struct PackageMetadata {
    authors: Vec<String>,
    license: Option<String>,
    description: Option<String>,
}

impl PackageMetadata {
    /// Create a new [`PackageMetadata`].
    pub fn new(authors: Vec<String>, license: Option<String>, description: Option<String>) -> Self {
        Self {
            authors,
            license,
            description,
        }
    }

    /// Get the package license
    pub fn license(&self) -> Option<&str> {
        self.license.as_deref()
    }

    /// Get the package authors
    pub fn authors(&self) -> &[String] {
        &self.authors
    }

    /// Get the package description
    pub fn description(&self) -> Option<&str> {
        self.description.as_deref()
    }

    pub fn into_authors(self) -> Vec<String> {
        self.authors
    }
}
