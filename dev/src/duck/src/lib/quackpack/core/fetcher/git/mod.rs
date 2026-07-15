//! Module responsible for interacting with Git repositories.

use std::path::Path;

use git2::build::RepoBuilder;
use git2::{Cred, CredentialType, FetchOptions, Oid, RemoteCallbacks, Repository};
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
pub struct GitClient<'duck> {
    ctx: &'duck DuckContext,
}

/// Maximal number of authentication attempts.
static MAX_AUTHENTICATION_NUMBER: u32 = 3;

impl<'duck> GitClient<'duck> {
    pub fn new(ctx: &'duck DuckContext) -> Self {
        Self { ctx }
    }

    /// Clone a repository pointed by `source` into `destination`, and parse a package it contains.
    #[tracing::instrument(skip(self, url) fields(url = url.as_str()))]
    pub fn clone_blocking(
        &self,
        url: &Url,
        reference: GitReference,
        destination: &Path,
    ) -> QuackResult<GitCloneResponse> {
        let mut builder = RepoBuilder::new();

        builder.fetch_options(self.fetch_options_for(url, reference));
        // git2-rs doesn't support cloning with a given tag :(.
        if let GitReference::Branch(branch) = reference {
            builder.branch(branch.as_str());
        }

        let repository = match builder.clone(url.as_str(), destination) {
            Ok(repository) => repository,
            Err(e) => {
                debug!("failed to clone: {e}");
                self.ctx.console().info(format!("failed to clone: {e}"))?;
                self.ctx
                    .console()
                    .info("retrying with a full clone instead of shallow clone")?;
                // We've failed to clone a repository, try to fallback to a non-shallow clone.
                // @TODO: #3146 Change this to only retry clone if the error could be from unsupporting shallow clone.
                if matches!(e.code(), git2::ErrorCode::Auth) {
                    return Err(e.into());
                }
                if !Self::can_shallow_clone(url, reference) {
                    return Err(e.into());
                }
                let mut fetch_options = self.fetch_options_for(url, reference);
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
        let package = PackageLoader::find_at_exact_directory(destination, self.ctx)
            .with_context(|| format!("git repository at `{}` is not a duckling package", url))?
            .into_package()
            .unwrap_package();
        Ok(GitCloneResponse {
            commit_hash: commit.to_string().into(),
            package,
        })
    }

    /// Get specific [`FetchOptions`] for cloning the given `url` with `reference`.
    fn fetch_options_for(&self, url: &Url, reference: GitReference) -> FetchOptions<'_> {
        let mut fetch_options = FetchOptions::new();
        if Self::can_shallow_clone(url, reference) {
            fetch_options.depth(1);
        }

        fetch_options.remote_callbacks(self.callbacks());
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
    fn callbacks(&self) -> RemoteCallbacks<'_> {
        let attempts = std::cell::RefCell::new(0);
        let mut callbacks = RemoteCallbacks::new();
        callbacks.credentials(move |_url, username_from_url, cred_types| {
            let mut attempts = attempts.borrow_mut();
            *attempts += 1;
            if *attempts > MAX_AUTHENTICATION_NUMBER {
                let mut err = git2::Error::from_str("too many authentication attempts. Make sure the repository supports chosen authentication method.");
                err.set_code(git2::ErrorCode::Auth);
                err.set_class(git2::ErrorClass::Callback);
            }
            if cred_types.contains(CredentialType::DEFAULT) {
                Cred::default()
            } else if cred_types.contains(CredentialType::SSH_KEY) {
                Self::ssh_callback(username_from_url)
            } else if cred_types.contains(CredentialType::USER_PASS_PLAINTEXT) {
                self.username_and_password_callback()
            } else {
                Err(git2::Error::from_str("unsupported authentication method"))
            }
        });
        callbacks
    }

    /// Callback for ssh authentication.
    fn ssh_callback(username_from_url: Option<&str>) -> Result<Cred, git2::Error> {
        let username = username_from_url.unwrap_or("git");
        Cred::ssh_key_from_agent(username)
    }

    /// Callback for simple username + password authentication.
    fn username_and_password_callback(&self) -> Result<Cred, git2::Error> {
        let username = self
            .ctx
            .console()
            .prompt_once("username: ")
            .map_err(|_| git2::Error::from_str("failed to get username"))?;
        let password = self
            .ctx
            .console()
            .password_once("password: ")
            .map_err(|_| git2::Error::from_str("failed to get password"))?;
        Cred::userpass_plaintext(&username, &password)
    }
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
