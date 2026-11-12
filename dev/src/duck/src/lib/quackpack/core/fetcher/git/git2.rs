// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module responsible for interacting with git repositories via [`git2`].

use std::cell::RefCell;
use std::path::Path;
use std::rc::Rc;

use git2::build::RepoBuilder;
use git2::{Cred, CredentialType, FetchOptions, Oid, RemoteCallbacks, SubmoduleUpdateOptions};
use tracing::{debug, trace};
use url::Url;

use super::{GitClient, GitRepository, GitSubmoduleOptions, can_shallow_clone};
use crate::quackpack::core::GitReference;
use crate::quackpack::util::to_url::ToUrl;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, StrId};

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

#[derive(Clone, Copy)]
/// Summary and data of called functions in [`RemoteCallbacks::credentials`].
struct GitCredentialsCallbackData<'a> {
    /// Maximal number of tried callbacks.
    max_auth_number: u32,
    /// Number of actually executed callbacks.
    attempted_auth_callbacks: u32,
    /// `true` if have tried SSH.
    tried_ssh: bool,
    /// `true` if have tried plaintext password.
    tried_plaintext: bool,
    /// `true` if have tried default credentials helper.
    tried_default: bool,

    /// Config for used in credentials.
    config: &'a git2::Config,
}

impl<'a> GitCredentialsCallbackData<'a> {
    /// Create a new [`GitCredentialsCallbackData`], with maximal authentication number,
    /// [`git2::Config`], and defaults.
    fn new(max_auth_number: u32, config: &'a git2::Config) -> Self {
        Self {
            max_auth_number,
            attempted_auth_callbacks: 0,
            tried_ssh: false,
            tried_plaintext: false,
            tried_default: false,
            config,
        }
    }

    /// Returns `true` if we can proceed to shallow clone.
    ///
    /// We might not want to proceed, if we failed due to authentication.
    fn should_try_shallow_clone(&self) -> bool {
        self.attempted_auth_callbacks == 0
    }

    /// Add notes about appropriate failures to the [`QuackError`].
    fn add_notes_to_clone_error(&self, error: &mut QuackError) {
        if self.tried_ssh {
            error.add_note("tried SSH authentication, but failed");
        }
        if self.tried_plaintext {
            error.add_note("tried username + password authentication, but failed");
        }
        if self.tried_default {
            error.add_note("tried git's credential helper authentication, but failed");
        }
    }
}

/// Maximal number of authentication attempts.
const MAX_AUTHENTICATION_NUMBER: u32 = 3;

impl<'duck> Git2Client<'duck> {
    pub fn new(ctx: &'duck DuckContext) -> Self {
        Self { ctx }
    }

    /// Get specific [`FetchOptions`] for cloning the given `url` with `reference`.
    fn fetch_options_for<'a>(
        &'a self,
        url: &Url,
        reference: GitReference,
        config: &'a git2::Config,
    ) -> (
        FetchOptions<'a>,
        Rc<RefCell<GitCredentialsCallbackData<'a>>>,
    ) {
        let mut fetch_options = FetchOptions::new();
        if can_shallow_clone(url, reference) {
            trace!("using a shallow clone");
            fetch_options.depth(1);
        }

        let (callbacks, data) = callbacks(self.ctx, config, url);
        fetch_options.remote_callbacks(callbacks);
        (fetch_options, data)
    }
}

/// Load a global [`git2::Config`].
fn load_git2_config() -> QuackResult<git2::Config> {
    git2::Config::open_default().context("failed to load default git config")
}

/// Create callbacks for authentication.
fn callbacks<'a>(
    ctx: &'a DuckContext,
    config: &'a git2::Config,
    url: &Url,
) -> (
    RemoteCallbacks<'a>,
    Rc<RefCell<GitCredentialsCallbackData<'a>>>,
) {
    let mut callbacks = RemoteCallbacks::new();
    let data = GitCredentialsCallbackData::new(MAX_AUTHENTICATION_NUMBER, config);
    let data = Rc::new(RefCell::new(data));
    let data_copy = Rc::clone(&data);
    callbacks.credentials(move |url, username_from_url, cred_types| {
            let mut data = data_copy.borrow_mut();
            data.attempted_auth_callbacks += 1;
            if data.attempted_auth_callbacks > data.max_auth_number {
                let mut err = git2::Error::from_str("too many authentication attempts; make sure the repository supports chosen authentication method");
                err.set_code(git2::ErrorCode::Auth);
                err.set_class(git2::ErrorClass::Callback);
                return Err(err);
            }
            if !data.tried_ssh && cred_types.contains(CredentialType::SSH_KEY) {
            data.tried_ssh = true;
                ssh_callback(username_from_url)
            } else if !data.tried_plaintext && cred_types.contains(CredentialType::USER_PASS_PLAINTEXT) {
            data.tried_plaintext = true;
            git2::Cred::credential_helper(data.config, url, username_from_url)
        } else if !data.tried_default && cred_types.contains(CredentialType::DEFAULT) {
            data.tried_default = true;
        Cred::default()
            } else {
                Err(git2::Error::from_str("unsupported authentication method"))
            }
        });
    let port = url.port();
    callbacks.certificate_check(move |cert, host| {
        super::known_hosts::check_certificate(ctx, cert, host, port)
    });
    (callbacks, data)
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
        let config = load_git2_config()?;
        let mut builder = RepoBuilder::new();

        let (opts, data) = self.fetch_options_for(url, reference, &config);
        builder.fetch_options(opts);
        // git2-rs doesn't support cloning with a given tag :(.
        if let GitReference::Branch(branch) = reference {
            builder.branch(branch.as_str());
        }

        let repository = match builder.clone(url.as_str(), destination) {
            Ok(repository) => repository,
            Err(e) => {
                debug!(error = %e, "failed to clone the repository");
                // We've failed to clone a repository, try to fallback to a non-shallow clone.
                if matches!(e.code(), git2::ErrorCode::Auth) {
                    return Err(e.into());
                }
                if !can_shallow_clone(url, reference) {
                    return Err(e.into());
                }
                if !data.borrow().should_try_shallow_clone() {
                    let mut e = e.into();
                    data.borrow().add_notes_to_clone_error(&mut e);
                    return Err(e);
                }
                debug!("falling back to a full clone");
                // We print this only here because otherwise the user gets information from the error.
                self.ctx.warning(format!(
                    "failed to shallow clone the repository at `{url}`: {e}"
                ))?;
                self.ctx
                    .info_verbose("retrying with a full clone instead of a shallow clone")?;
                let (mut fetch_options, data) = self.fetch_options_for(url, reference, &config);
                // Here we always want to perform full clone, so set depth to 0.
                fetch_options.depth(0);
                builder.fetch_options(fetch_options);
                builder.clone(url.as_str(), destination).map_err(|e| {
                    let mut e = e.into();
                    data.borrow().add_notes_to_clone_error(&mut e);
                    e
                })?
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
        let config = self.repository.config().with_context(|| {
            format!(
                "failed to load a config of a repository `{}`",
                self.repository.path().display()
            )
        })?;
        match options {
            GitSubmoduleOptions::Init => initialize_submodules(&self.repository),
            GitSubmoduleOptions::Update { init, recursive } => {
                update_submodules(&self.repository, init, recursive, self.ctx, &config)
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
    config: &git2::Config,
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
        let data = if let Some(url) = url
            && let Ok(url) = url.to_url()
        {
            let mut fetch_options = FetchOptions::new();
            let (callbacks, data) = callbacks(ctx, config, &url);
            fetch_options.remote_callbacks(callbacks);
            options.fetch(fetch_options);
            Some(data)
        } else {
            None
        };

        submodule
            .update(init, Some(&mut options))
            .with_context(|| format!("failed to update a submodule `{submodule_name}`"))
            .map_err(|mut e| {
                if let Some(data) = data {
                    data.borrow().add_notes_to_clone_error(&mut e);
                };
                e
            })?;

        if !recursive {
            continue;
        }

        let subrepository = submodule.open().with_context(|| {
            format!("failed to open a repository of a submodule `{submodule_name}`")
        })?;

        update_submodules(&subrepository, init, recursive, ctx, config).with_context(|| {
            format!("failed to recursively update submodules of a submodule `{submodule_name}`")
        })?;
    }
    Ok(())
}
