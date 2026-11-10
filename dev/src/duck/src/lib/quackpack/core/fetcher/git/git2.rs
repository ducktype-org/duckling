// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module responsible for interacting with git repositories via [`git2`].

use std::path::Path;

use git2::build::RepoBuilder;
use git2::{Cred, CredentialType, FetchOptions, Oid, RemoteCallbacks, Repository};
use tracing::{debug, trace};
use url::Url;

use super::{GitClient, GitRepository, can_shallow_clone};
use crate::quackpack::core::GitReference;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId};

#[derive(Debug)]
/// Client implementing interaction with Git repositories.
/// Currently it provides only static methods.
pub struct Git2Client<'duck> {
    ctx: &'duck DuckContext,
}

/// Maximal number of authentication attempts.
const MAX_AUTHENTICATION_NUMBER: u32 = 3;

impl<'duck> Git2Client<'duck> {
    pub fn new(ctx: &'duck DuckContext) -> Self {
        Self { ctx }
    }

    /// Get specific [`FetchOptions`] for cloning the given `url` with `reference`.
    fn fetch_options_for(&self, url: &Url, reference: GitReference) -> FetchOptions<'_> {
        let mut fetch_options = FetchOptions::new();
        if can_shallow_clone(url, reference) {
            trace!(target: "duck::git", "using a shallow clone");
            fetch_options.depth(1);
        }

        fetch_options.remote_callbacks(self.callbacks(url));
        fetch_options
    }

    /// Create callbacks for authentication.
    fn callbacks(&self, url: &Url) -> RemoteCallbacks<'_> {
        let attempts = std::cell::RefCell::new(0);
        let mut callbacks = RemoteCallbacks::new();
        callbacks.credentials(move |_url, username_from_url, cred_types| {
            let mut attempts = attempts.borrow_mut();
            *attempts += 1;
            if *attempts > MAX_AUTHENTICATION_NUMBER {
                let mut err = git2::Error::from_str("too many authentication attempts; make sure the repository supports chosen authentication method");
                err.set_code(git2::ErrorCode::Auth);
                err.set_class(git2::ErrorClass::Callback);
                return Err(err);
            }
            if cred_types.contains(CredentialType::DEFAULT) {
                Cred::default()
            } else if cred_types.contains(CredentialType::SSH_KEY) {
                ssh_callback(username_from_url)
            } else if cred_types.contains(CredentialType::USER_PASS_PLAINTEXT) {
                self.username_and_password_callback()
            } else {
                Err(git2::Error::from_str("unsupported authentication method"))
            }
        });
        let port = url.port();
        callbacks.certificate_check(move |cert, host| {
            super::known_hosts::check_certificate(self.ctx, cert, host, port)
        });
        callbacks
    }

    /// Callback for simple username + password authentication.
    fn username_and_password_callback(&self) -> Result<Cred, git2::Error> {
        let username = self
            .ctx
            .prompt_once("username")
            .map_err(|_| git2::Error::from_str("failed to get username"))?;
        let password = self
            .ctx
            .password_once("password")
            .map_err(|_| git2::Error::from_str("failed to get password"))?;
        Cred::userpass_plaintext(&username, &password)
    }
}

/// Callback for ssh authentication.
fn ssh_callback(username_from_url: Option<&str>) -> Result<Cred, git2::Error> {
    let username = username_from_url.unwrap_or("git");
    Cred::ssh_key_from_agent(username)
}

impl GitClient for Git2Client<'_> {
    fn clone(
        &self,
        url: &Url,
        reference: GitReference,
        destination: &Path,
    ) -> QuackResult<Box<dyn GitRepository>> {
        let mut builder = RepoBuilder::new();

        builder.fetch_options(self.fetch_options_for(url, reference));
        // git2-rs doesn't support cloning with a given tag :(.
        if let GitReference::Branch(branch) = reference {
            builder.branch(branch.as_str());
        }

        let repository = match builder.clone(url.as_str(), destination) {
            Ok(repository) => repository,
            Err(e) => {
                debug!(target: "duck::git", error = %e, "failed to clone the repository");
                // We've failed to clone a repository, try to fallback to a non-shallow clone.
                if matches!(e.code(), git2::ErrorCode::Auth) {
                    return Err(e.into());
                }
                if !can_shallow_clone(url, reference) {
                    return Err(e.into());
                }
                debug!(target: "duck::git", "falling back to a full clone");
                // We print this only here because otherwise the user gets information from the error.
                self.ctx.warning(format!(
                    "failed to shallow clone the repository at `{url}`: {e}"
                ))?;
                self.ctx
                    .info_verbose("retrying with a full clone instead of a shallow clone")?;
                let mut fetch_options = self.fetch_options_for(url, reference);
                // Here we always want to perform full clone, so set depth to 0.
                fetch_options.depth(0);
                builder.fetch_options(fetch_options);
                builder.clone(url.as_str(), destination)?
            }
        };
        Ok(Box::new(repository))
    }

    fn uses_cli(&self) -> bool {
        false
    }
}

impl GitRepository for git2::Repository {
    fn checkout(&self, reference: GitReference) -> QuackResult<()> {
        if let GitReference::Rev(commit) = reference {
            self.checkout_commit(commit.as_str())
                .with_context(|| format!("failed to check out the commit `{}`", commit))?;
        } else if let GitReference::Tag(tag) = reference {
            self.checkout_tag(tag.as_str())
                .with_context(|| format!("failed to check out the tag `{}`", tag))?;
        }
        Ok(())
    }

    fn head_commit(&self) -> QuackResult<StrId> {
        let commit = self
            .head()
            .context("failed to get HEAD")?
            .peel_to_commit()
            .context("failed to peel HEAD to a commit")?
            .id();
        Ok(commit.to_string().into())
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
    fn checkout_commit(&self, commit: &str) -> QuackResult<()> {
        let oid =
            Oid::from_str(commit).with_context(|| format!("`{commit}` is not a valid Oid"))?;
        let commit = self
            .find_commit(oid)
            .with_context(|| format!("repository does not have a commit `{}", commit))?;
        let tree = commit
            .tree()
            .context("failed to get tree of a repository")?;
        self.checkout_tree(tree.as_object(), None)
            .context("failed to checkout the tree")?;
        self.set_head_detached(oid)
            .context("failed to set detached HEAD")?;
        Ok(())
    }

    fn checkout_tag(&self, tag: &str) -> QuackResult<()> {
        let refname = format!("refs/tags/{}", tag);
        let reference = self
            .find_reference(&refname)
            .with_context(|| format!("repository does not have a tag `{}`", tag))?;

        let commit = reference
            .peel_to_commit()
            .with_context(|| format!("failed to get a commit of tag `{tag}`"))?;

        self.checkout_tree(commit.as_object(), None)
            .context("failed to checkout the tree")?;

        self.set_head_detached(commit.id())
            .context("failed to set detached HEAD")?;
        Ok(())
    }
}
