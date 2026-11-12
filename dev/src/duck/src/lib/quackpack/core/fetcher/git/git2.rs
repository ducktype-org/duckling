// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module responsible for interacting with git repositories via [`git2`].

use std::path::Path;

use git2::build::RepoBuilder;
use git2::{Cred, CredentialType, FetchOptions, Oid, RemoteCallbacks, SubmoduleUpdateOptions};
use tracing::{debug, trace};
use url::Url;

use super::{GitClient, GitRepository, GitSubmoduleOptions, can_shallow_clone};
use crate::quackpack::core::GitReference;
use crate::quackpack::util::to_url::ToUrl;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId};

/// An implementation of [`GitRepository`], which also keeps [`DuckContext`].
struct Git2Repository<'duck> {
    ctx: &'duck DuckContext,
    repository: git2::Repository,
}

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

        fetch_options.remote_callbacks(callbacks(self.ctx, url));
        fetch_options
    }
}

/// Create callbacks for authentication.
fn callbacks<'duck>(ctx: &'duck DuckContext, url: &Url) -> RemoteCallbacks<'duck> {
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
                username_and_password_callback(ctx)
            } else {
                Err(git2::Error::from_str("unsupported authentication method"))
            }
        });
    let port = url.port();
    callbacks.certificate_check(move |cert, host| {
        super::known_hosts::check_certificate(ctx, cert, host, port)
    });
    callbacks
}

/// Callback for simple username + password authentication.
fn username_and_password_callback(ctx: &DuckContext) -> Result<Cred, git2::Error> {
    let username = ctx
        .prompt_once("username")
        .map_err(|_| git2::Error::from_str("failed to get username"))?;
    let password = ctx
        .password_once("password")
        .map_err(|_| git2::Error::from_str("failed to get password"))?;
    Cred::userpass_plaintext(&username, &password)
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
    ) -> QuackResult<Box<dyn GitRepository + '_>> {
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
        Ok(Box::new(Git2Repository {
            repository,
            ctx: self.ctx,
        }))
    }

    fn uses_cli(&self) -> bool {
        false
    }
}

impl GitRepository for Git2Repository<'_> {
    fn checkout(&self, reference: GitReference) -> QuackResult<()> {
        // We only checkout to tag or commit, since we can clone to branch.
        if let GitReference::Rev(commit) = reference {
            checkout_commit(&self.repository, commit.as_str())
        } else if let GitReference::Tag(tag) = reference {
            checkout_tag(&self.repository, tag.as_str())
        } else {
            Ok(())
        }
    }

    fn head_commit(&self) -> QuackResult<StrId> {
        let commit = self
            .repository
            .head()
            .context("failed to get HEAD")?
            .peel_to_commit()
            .context("failed to peel HEAD to a commit")?
            .id();
        Ok(commit.to_string().into())
    }

    fn init_submodules(&self, options: GitSubmoduleOptions) -> QuackResult<()> {
        match options {
            GitSubmoduleOptions::Init => initialize_submodules(&self.repository),
            GitSubmoduleOptions::Update { init, recursive } => {
                update_submodules(&self.repository, init, recursive, self.ctx)
            }
        }
    }
}

/// Checkout [`git2::Repository`] into a given commit.
fn checkout_commit(repository: &git2::Repository, commit: &str) -> QuackResult<()> {
    let oid = Oid::from_str(commit).with_context(|| format!("`{commit}` is not a valid Oid"))?;
    let commit = repository
        .find_commit(oid)
        .with_context(|| format!("repository does not have a commit `{}", commit))?;
    let tree = commit
        .tree()
        .context("failed to get tree of a repository")?;
    repository
        .checkout_tree(tree.as_object(), None)
        .context("failed to checkout the tree")?;
    repository
        .set_head_detached(oid)
        .context("failed to set detached HEAD")?;
    Ok(())
}

/// Checkout [`git2::Repository`] into a given tag.
fn checkout_tag(repository: &git2::Repository, tag: &str) -> QuackResult<()> {
    let refname = format!("refs/tags/{}", tag);
    let reference = repository
        .find_reference(&refname)
        .with_context(|| format!("repository does not have a tag `{}`", tag))?;

    let commit = reference
        .peel_to_commit()
        .with_context(|| format!("failed to get a commit of tag `{tag}`"))?;

    repository
        .checkout_tree(commit.as_object(), None)
        .context("failed to checkout the tree")?;

    repository
        .set_head_detached(commit.id())
        .context("failed to set detached HEAD")?;
    Ok(())
}

/// Initialise top-level submodules of the repository.
///
/// This function doesn't update them, nor is recursive.
///
/// Use [`update_submodules`] for more granular control.
fn initialize_submodules(repository: &git2::Repository) -> QuackResult<()> {
    for mut submodule in repository
        .submodules()
        .context("failed to load submodules")?
    {
        let submodule_name = submodule.name_bytes();
        let submodule_name = String::from_utf8_lossy(submodule_name).to_string();
        submodule
            .init(false)
            .with_context(|| format!("failed to initialize a submodule `{submodule_name}`"))?;
    }
    Ok(())
}

/// Update git submodules.
///
/// If `init` is true, also initialise them.
/// If `recursive` is true, also recur into submodules.
fn update_submodules(
    repository: &git2::Repository,
    init: bool,
    recursive: bool,
    ctx: &DuckContext,
) -> QuackResult<()> {
    for mut submodule in repository
        .submodules()
        .context("failed to load submodules")?
    {
        let submodule_name = submodule.name_bytes();
        let submodule_name = String::from_utf8_lossy(submodule_name).to_string();

        let mut options = SubmoduleUpdateOptions::new();
        // There are submodules without URLs...
        // They are indicated by `Ok(None)`.
        let url = submodule
            .url()
            .with_context(|| format!("failed to get URL of a submodule `{submodule_name}`"))?;
        if let Some(url) = url
            && let Ok(url) = url.to_url()
        {
            let mut fetch_options = FetchOptions::new();
            fetch_options.remote_callbacks(callbacks(ctx, &url));
            options.fetch(fetch_options);
        }

        submodule
            .update(init, Some(&mut options))
            .with_context(|| format!("failed to update a submodule `{submodule_name}`"))?;

        if !recursive {
            continue;
        }

        let subrepository = submodule.open().with_context(|| {
            format!("failed to open a repository of a submodule `{submodule_name}`")
        })?;

        update_submodules(&subrepository, init, recursive, ctx).with_context(|| {
            format!("failed to recursively update submodules of a submodule `{submodule_name}`")
        })?;
    }
    Ok(())
}
