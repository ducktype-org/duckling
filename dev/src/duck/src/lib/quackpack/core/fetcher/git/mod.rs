//! Module responsible for interacting with Git repositories.

use std::path::Path;

use git2::build::RepoBuilder;
use git2::{Cred, FetchOptions, Oid, RemoteCallbacks, Repository};
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

        builder.fetch_options(fetch_options_for(url, reference, ctx));
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
                let mut fetch_options = fetch_options_for(url, reference, ctx);
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
fn fetch_options_for<'duck>(
    url: &Url,
    reference: GitReference,
    ctx: &'duck DuckContext,
) -> FetchOptions<'duck> {
    let mut fetch_options = FetchOptions::new();
    if can_shallow_clone(url, reference) {
        fetch_options.depth(1);
    }

    fetch_options.remote_callbacks(callbacks(url, ctx));
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

/// Create callbacks for authentication.
fn callbacks<'duck>(url: &Url, ctx: &'duck DuckContext) -> RemoteCallbacks<'duck> {
    let mut callbacks = RemoteCallbacks::new();
    match url.scheme() {
        "ssh" => setup_ssh_authentication(&mut callbacks),
        "http" => setup_http_authentication(&mut callbacks, ctx),
        _ => {}
    };
    callbacks
}

/// Setup ssh authentication.
fn setup_ssh_authentication(callbacks: &mut RemoteCallbacks) {
    callbacks.credentials(|_url, username_from_url, _allowed_types| {
        let username = username_from_url.unwrap_or("git");
        Cred::ssh_key_from_agent(username)
    });
}

/// Setup simple username + password authentication.
fn setup_http_authentication<'duck>(
    callbacks: &mut RemoteCallbacks<'duck>,
    ctx: &'duck DuckContext,
) {
    callbacks.credentials(|_url, _username_from_url, _allowed_types| {
        let username = ctx
            .console()
            .prompt_once("username: ")
            .map_err(|_| git2::Error::from_str("failed to get username"))?;
        let password = ctx
            .console()
            .password_once("password: ")
            .map_err(|_| git2::Error::from_str("failed to get password"))?;
        Cred::userpass_plaintext(&username, &password)
    });
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
