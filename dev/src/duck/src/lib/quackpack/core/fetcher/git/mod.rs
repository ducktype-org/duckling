//! Module responsible for interacting with Git repositories.

use std::path::Path;

use git2::build::RepoBuilder;
use git2::{FetchOptions, Oid, Repository};
use tracing::debug;
use url::Url;

use crate::quackpack::core::fetcher::types::GitCloneResponse;
use crate::quackpack::core::{GitReference, PackageLoader};
use crate::quackpack::util::is_local_file::IsLocalFile;
use crate::{DuckContext, QuackResult, QuackResultContext};

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// Client implementing interaction with Git repositories.
/// Currently it provides only static methods.
pub struct GitClient {}

impl GitClient {
    /// Clone a repository pointed by `source` into `destination`, and parse a package it contains.
    #[tracing::instrument(skip(ctx, url) fields(url = url.as_str()))]
    pub fn clone_blocking(
        url: &Url,
        reference: GitReference,
        destination: &Path,
        ctx: &DuckContext,
    ) -> QuackResult<GitCloneResponse> {
        let mut builder = RepoBuilder::new();

        builder.fetch_options(fetch_options_for(url, reference));
        // git2-rs doesn't support cloning with a given tag :(.
        if let GitReference::Branch(branch) = reference {
            builder.branch(branch.as_str());
        }

        let repository = match builder.clone(url.as_str(), destination) {
            Ok(repository) => repository,
            Err(e) => {
                debug!("failed to clone: {e}");
                // We've failed to clone a repository, try to fallback to a non-shallow clone.
                if !can_shallow_clone(url, reference) {
                    return Err(e.into());
                }
                let mut fetch_options = fetch_options_for(url, reference);
                fetch_options.depth(0);
                builder.fetch_options(fetch_options);
                builder.clone(url.as_str(), destination)?
            }
        };

        debug!("will checkout to tag...");

        // Prefer specific commits over tags.
        if let GitReference::Rev(commit) = reference {
            repository.checkout_commit(commit.as_str()).with_context(|| {
                format!(
                    "when performing a checkout of a repository cloned from `{}` to a commit `{}`",
                    url,
                    commit
                )
            })?;
        } else if let GitReference::Tag(tag) = reference {
            repository.checkout_tag(tag.as_str()).with_context(|| {
                format!(
                    "when performing a checkout of a repository cloned from `{}` to a tag `{}`",
                    url, tag
                )
            })?;
        }

        let commit = repository.head()?.peel_to_commit()?.id();
        let package = PackageLoader::find_at_exact_directory(destination, ctx)
            .with_context(|| format!("git repository at `{}` is not a duckling package", url))?
            .into_package()
            .unwrap_package();
        Ok(GitCloneResponse {
            commit_hash: commit.to_string().into(),
            package,
        })
    }
}

/// Get specific [`FetchOptions`] for cloning the given `url` with `reference`.
fn fetch_options_for(url: &Url, reference: GitReference) -> FetchOptions<'static> {
    let mut fetch_options = FetchOptions::new();
    if can_shallow_clone(url, reference) {
        fetch_options.depth(1);
    }
    fetch_options
}

/// Check, if we can shallow clone a `reference` from `url`.
/// Right now conditions are as follow:
/// 1. `url` must not be a local repository (i.e. schema != "file"),
/// 2. reference must NOT point to specific commit (it's either a default branch, or a specific branch).
fn can_shallow_clone(url: &Url, reference: GitReference) -> bool {
    let is_local_repository_url = url.is_local_file();
    !is_local_repository_url && (reference.is_default() || reference.is_branch())
}

/// A helper trait for repository methods.
trait RepositoryExt {
    /// Checkout `self` into a given commit.
    fn checkout_commit(&self, commit: &str) -> QuackResult<()>;
    /// Checkout `self` into a given tag.
    fn checkout_tag(&self, tag: &str) -> QuackResult<()>;
}

impl RepositoryExt for Repository {
    #[tracing::instrument(skip(self))]
    fn checkout_commit(&self, commit: &str) -> QuackResult<()> {
        let oid =
            Oid::from_str(commit).with_context(|| format!("`{commit}` is not a valid Oid"))?;
        let commit = self
            .find_commit(oid)
            .with_context(|| format!("repository does not have a commit `{}", commit))?;
        let tree = commit.tree()?;
        self.checkout_tree(tree.as_object(), None)?;
        self.set_head_detached(oid)?;
        Ok(())
    }

    #[tracing::instrument(skip(self))]
    fn checkout_tag(&self, tag: &str) -> QuackResult<()> {
        let refname = format!("refs/tags/{}", tag);
        let reference = self
            .find_reference(&refname)
            .with_context(|| format!("repository does not have a tag `{}`", tag))?;

        let commit = reference.peel_to_commit()?;

        self.checkout_tree(commit.as_object(), None)?;

        self.set_head_detached(commit.id())?;
        Ok(())
    }
}
