//! A helper trait for getting URLs for communicating with a registry instance.
use url::form_urlencoded::Serializer;
use url::{Url, UrlQuery};

use crate::QuackResult;
use crate::quackpack::core::fetcher::types;

/// A helper trait for getting URLs for communicating with a registry instance.
pub trait UrlExt: Sized {
    /// An internal method.
    fn _join(&self, path: &str) -> QuackResult<Self>;

    /// An internal method.
    fn _query_pairs_mut(&mut self) -> Serializer<'_, UrlQuery<'_>>;

    /// Get the URL for querying multimetadata of the package `package_name`.
    fn for_multi_metadata(&self, package_name: &str) -> QuackResult<Self> {
        self._join(&format!("/packages/{package_name}"))
    }

    /// Get the URL for querying exact metadata of the package `package`.
    fn for_exact_metadata(&self, package: &types::Package) -> QuackResult<Self> {
        self._join(&format!("/packages/{}/{}", package.name, package.version))
    }

    /// Get the URL for downloading a blob of the package `package`.
    ///
    /// A blob is a tar gunziped directory with a package's source code.
    fn for_blob(&self, package: &types::Package) -> QuackResult<Self> {
        self._join(&format!(
            "/packages/{}/{}/download",
            package.name, package.version
        ))
    }

    /// Get the URL for searching for the given query.
    fn for_search(&self, query: &str) -> QuackResult<Self> {
        let mut new = self._join("/packages")?;
        new._query_pairs_mut().append_pair("q", query);
        Ok(new)
    }

    #[allow(dead_code)]
    /// Get the URL for publishing a new package.
    fn for_new_package(&self) -> Self {
        self._join("/packages")
            .expect("we control queries statically...?")
    }

    #[allow(dead_code)]
    /// Get the URL for uploading a package's blob.
    fn for_new_blob(&self, package: &types::Package) -> QuackResult<Self> {
        self._join(&format!("/packages/{}/{}", package.name, package.version))
    }
}

impl UrlExt for Url {
    fn _join(&self, path: &str) -> QuackResult<Self> {
        self.join(path).map_err(Into::into)
    }

    fn _query_pairs_mut(&mut self) -> Serializer<'_, UrlQuery<'_>> {
        self.query_pairs_mut()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn basic_endpoints() {
        let base = "https://localhost:9001";
        let package_name = "package";
        let package_version = "1.0.0";
        let url = base.to_url().unwrap();
        let package = types::Package {
            name: package_name.into(),
            version: package_version.parse().unwrap(),
        };
        assert_eq!(
            url.for_multi_metadata(package.name.as_str())
                .unwrap()
                .as_str(),
            format!("{base}/packages/{package_name}")
        );

        assert_eq!(
            url.for_exact_metadata(&package).unwrap().as_str(),
            format!("{base}/packages/{package_name}/{package_version}")
        );

        assert_eq!(
            url.for_blob(&package).unwrap().as_str(),
            format!("{base}/packages/{package_name}/{package_version}/download")
        );

        assert_eq!(
            url.for_search("simple_query").unwrap().as_str(),
            format!("{base}/packages?q=simple_query")
        );

        assert_eq!(
            url.for_search("foo bar").unwrap().as_str(),
            format!("{base}/packages?q=foo+bar")
        );

        assert_eq!(url.for_new_package().as_str(), format!("{base}/packages"));

        assert_eq!(
            url.for_new_blob(&package).unwrap().as_str(),
            format!("{base}/packages/{package_name}/{package_version}")
        );
    }
}
