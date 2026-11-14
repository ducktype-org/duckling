// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module responsible for interacting with git repositories via [`git2`].

use std::cell::RefCell;
use std::fmt;
use std::path::Path;
use std::rc::Rc;

use git2::build::RepoBuilder;
use git2::{Cred, CredentialType, FetchOptions, Oid, RemoteCallbacks, SubmoduleUpdateOptions};
use tracing::{debug, trace, warn};
use url::Url;

use super::{GitClient, GitRepository, GitSubmoduleOptions, can_shallow_clone};
use crate::quackpack::core::GitReference;
use crate::quackpack::util::to_url::ToUrl;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, StrId};

/// For some odd reason [`git2`] doesn't ship this type alias.
type GitResult<T> = Result<T, git2::Error>;

/// An implementation of [`GitRepository`], which also keeps [`DuckContext`].
struct Git2Repository<'duck> {
    ctx: &'duck DuckContext,
    repository: git2::Repository,
}

#[derive(Debug)]
/// Client implementing interaction with Git repositories.
pub struct Git2Client<'duck> {
    ctx: &'duck DuckContext,
}

#[derive(Default, Debug)]
/// Metadata about a given credential callback.
///
/// For more information about error handling and possible authentication errors, see
/// [`credentials_callback`].
struct CredentialData {
    /// Whether we tried this credential.
    tried: bool,
    /// Whether we failed this credential.
    maybe_failed: MaybeFailedCredential,
}

impl CredentialData {
    /// Maybe add a note about failed credential to the [`QuackError`].
    fn maybe_add_note(&self, error: &mut QuackError, credential_name: &str) {
        let note = format!("tried {credential_name} authentication but failed");
        if let MaybeFailedCredential::Failed(FailedCredential {
            error: ref git_error,
        }) = self.maybe_failed
        {
            error.add_note(format!("{note} because: {git_error}"));
        } else if self.tried {
            error.add_note(note);
        }
    }
}

/// Helper indicating whether a credential failed or not.
#[derive(Default, Debug)]
enum MaybeFailedCredential {
    /// We did not failed (or haven't tried).
    #[default]
    NotFailed,
    /// We failed this credential.
    Failed(FailedCredential),
}

impl MaybeFailedCredential {
    /// We have failed a credential. Mark it as failed.
    ///
    /// For more information about error handling and possible authentication errors, see
    /// [`credentials_callback`].
    fn fail(&mut self, error: &git2::Error) {
        if matches!(self, Self::NotFailed) {
            *self = Self::Failed(FailedCredential::from_error(error));
        }
    }

    /// Check if we failed a credential, and update metadata.
    ///
    /// For more information about error handling and possible authentication errors, see
    /// [`credentials_callback`].
    fn maybe_fail<T>(&mut self, result: &GitResult<T>) {
        if let Err(e) = result {
            warn!(%e, "callback failed");
            self.fail(e);
        }
    }

    /// An alternative to [`maybe_fail]`, which reduces code in
    /// [`credentials_callback`].
    ///
    /// For more information about error handling and possible authentication errors, see
    /// [`credentials_callback`].
    fn maybe_fail_callback<T>(&mut self, result: GitResult<T>) -> GitResult<T> {
        self.maybe_fail(&result);
        result
    }
}

/// Metadata about failed credential.
#[derive(Debug)]
struct FailedCredential {
    error: git2::Error,
}

impl FailedCredential {
    /// Create a new [`FailedCredential`] from the given error.
    fn from_error(error: &git2::Error) -> Self {
        let my_error = git2::Error::new(error.code(), error.class(), error.message());
        Self { error: my_error }
    }
}

/// Summary and data of called functions in [`RemoteCallbacks::credentials`].
struct GitCredentialsCallbackDataInner<'a> {
    /// Data about ssh authentication.
    ssh: CredentialData,
    /// Data about plaintext authentication.
    plaintext: CredentialData,
    /// Data about plaintext via credential helper authentication.
    cred_helper: CredentialData,
    /// Data about “default” authentication.
    default: CredentialData,

    /// Config for used in credentials.
    config: &'a git2::Config,

    /// Possible credentials supported by/requested by the server.
    possible_creds: CredentialType,
}

impl fmt::Debug for GitCredentialsCallbackDataInner<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("GitCredentialsCallbackData")
            .field("ssh", &self.ssh)
            .field("plaintext", &self.plaintext)
            .field("cred_helper", &self.cred_helper)
            .field("default", &self.default)
            .field("possible_creds", &self.possible_creds)
            .finish()
    }
}

type GitCredentialsCallbackData<'a> = Rc<RefCell<GitCredentialsCallbackDataInner<'a>>>;

impl<'a> GitCredentialsCallbackDataInner<'a> {
    /// Create a new [`GitCredentialsCallbackDataInner`], with [`git2::Config`] and defaults.
    fn new(config: &'a git2::Config) -> Self {
        Self {
            ssh: Default::default(),
            plaintext: Default::default(),
            cred_helper: Default::default(),
            default: Default::default(),
            possible_creds: CredentialType::empty(),
            config,
        }
    }

    /// Returns `true` if it's likely that a user has configured `credential.helper` and using it
    /// won't fail (or is less likely to fail).
    fn has_credential_helper(&self) -> bool {
        let result = self.config.get_str("credential.helper").is_ok();
        debug!(has_credential_helper = ?result);
        result
    }

    /// Returns `true` if we can proceed to full clone.
    ///
    /// We might not want to proceed, if we failed due to authentication.
    fn can_proceed_to_full_clone(&self) -> bool {
        !self.attempted_every_callback()
    }

    /// Returns `true` if attempted every supported authentication method (by both client (us) and
    /// server).
    fn attempted_every_callback(&self) -> bool {
        trace!(?self);
        if self.possible_creds.is_ssh_key() && !self.ssh.tried {
            debug!("could try SSH");
            return false;
        }

        if self.possible_creds.is_user_pass_plaintext() {
            if !self.plaintext.tried {
                debug!("could try username + password");
                return false;
            }
            if !self.cred_helper.tried && self.has_credential_helper() {
                debug!("could try credential helper");
                return false;
            }
        }

        if self.possible_creds.is_default() && !self.default.tried {
            debug!("could try default helper");
            return false;
        }
        true
    }

    /// Add notes about appropriate failures to the [`QuackError`].
    /// SSH and plaintext are treated in a special way:
    /// 1. for SSH, we want to add extra notes and hints if we fail,
    /// 2. for password it's more subtle: [`MaybeFailedCredential::maybe_fail`] gets result from
    ///    *creating* the [`git2::Cred`] object. We can create it successfully, but credentials may
    ///    just be wrong. Unfortunately, [`git2`] doesn't tell us about that.
    ///
    /// For more information about error handling and possible authentication errors, see
    /// [`credentials_callback`].
    fn add_notes_to_clone_error(&self, error: &mut QuackError) {
        self.ssh.maybe_add_note(error, "SSH");
        if self.ssh.tried {
            error.add_note("SSH keys need to be in an SSH agent to work");
            error.add_hint("try adding your SSH keys to the SSH agent");
        }
        self.plaintext.maybe_add_note(error, "username + password");
        if self.plaintext.tried {
            error.add_note("you either entered wrong credentials, or the repository does not support username + password authentication");
        }
        self.cred_helper
            .maybe_add_note(error, "git's credential helper (`credential.helper`)");
        self.default.maybe_add_note(error, "default");
    }
}

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
        old_data: Option<GitCredentialsCallbackData<'a>>,
    ) -> (FetchOptions<'a>, GitCredentialsCallbackData<'a>) {
        let mut fetch_options = FetchOptions::new();
        if can_shallow_clone(url, reference) {
            trace!("using a shallow clone");
            fetch_options.depth(1);
        }

        let (callbacks, data) = callbacks(self.ctx, config, url, old_data);
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
    old_data: Option<GitCredentialsCallbackData<'a>>,
) -> (RemoteCallbacks<'a>, GitCredentialsCallbackData<'a>) {
    let mut callbacks = RemoteCallbacks::new();
    let data = old_data.unwrap_or_else(|| {
        let data = GitCredentialsCallbackDataInner::new(config);
        Rc::new(RefCell::new(data))
    });
    let data_copy = Rc::clone(&data);
    callbacks.credentials(move |url, username_from_url, cred_types| {
        credentials_callback(ctx, url, username_from_url, cred_types, &data_copy)
    });
    let port = url.port();
    callbacks.certificate_check(move |cert, host| {
        super::known_hosts::check_certificate(ctx, cert, host, port)
    });
    (callbacks, data)
}

/// Callback for [`RemoteCallbacks::credentials`].
///
/// General note on error handling: we communicate to [`git2`] custom errors/errors that occurred
/// while creating an object (e.g. [`git2::Cred::credential_helper`] reads git config, but may fail
/// to find a field). Unfortunately, some errors (e.g. wrong username + password) are detected later
/// by [`git2`], but it doesn't have an API to tell us about them, so we lack diagnostic. (from my
/// understanding, [`git2`] sees that an authentication failed, so it tries another one, until we
/// return "unsupported authentication method" with the right code and class).
fn credentials_callback(
    ctx: &DuckContext,
    url: &str,
    username_from_url: Option<&str>,
    cred_types: CredentialType,
    data: &GitCredentialsCallbackData<'_>,
) -> GitResult<git2::Cred> {
    trace!(?cred_types);
    let mut data = data.borrow_mut();
    data.possible_creds = cred_types;
    if !data.ssh.tried && cred_types.is_ssh_key() {
        debug!("trying SSH authentication");
        data.ssh.tried = true;
        return data
            .ssh
            .maybe_failed
            .maybe_fail_callback(ssh_callback(username_from_url));
    }

    if cred_types.is_user_pass_plaintext() {
        if !data.plaintext.tried {
            debug!("trying username + password authentication");
            data.plaintext.tried = true;
            return data
                .plaintext
                .maybe_failed
                .maybe_fail_callback(username_and_password_callback(ctx));
        }
        if !data.cred_helper.tried && data.has_credential_helper() {
            debug!("trying credential helper authentication");
            data.cred_helper.tried = true;
            let result = git2::Cred::credential_helper(data.config, url, username_from_url);
            return data.cred_helper.maybe_failed.maybe_fail_callback(result);
        }
    }

    if !data.default.tried && cred_types.is_default() {
        debug!("trying default authentication");
        data.default.tried = true;
        return data
            .default
            .maybe_failed
            .maybe_fail_callback(Cred::default());
    }

    Err(git2::Error::new(
        git2::ErrorCode::Auth,
        git2::ErrorClass::Callback,
        "unsupported authentication method",
    ))
}

/// Callback for ssh authentication.
fn ssh_callback(username_from_url: Option<&str>) -> GitResult<Cred> {
    let username = username_from_url.unwrap_or("git");
    Cred::ssh_key_from_agent(username)
}

/// Callback for simple username + password authentication.
fn username_and_password_callback(ctx: &DuckContext) -> GitResult<Cred> {
    let username = ctx
        .prompt_once("username")
        .map_err(|_| git2::Error::from_str("failed to get username"))?;
    let password = ctx
        .password_once("password")
        .map_err(|_| git2::Error::from_str("failed to get password"))?;
    Cred::userpass_plaintext(&username, &password)
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

        let (opts, data) = self.fetch_options_for(url, reference, &config, None);
        builder.fetch_options(opts);
        // git2-rs doesn't support cloning with a given tag :(.
        if let GitReference::Branch(branch) = reference {
            builder.branch(branch.as_str());
        }

        let repository = match builder.clone(url.as_str(), destination) {
            Ok(repository) => repository,
            Err(e) => {
                warn!(error = %e, "failed to clone the repository");
                trace!(data = ?data.borrow(), "callback data after shallow clone");
                // We've failed to clone a repository, try to fallback to a non-shallow clone.

                let can_perform_full_clone = data.borrow().can_proceed_to_full_clone()
                    && !matches!(e.code(), git2::ErrorCode::Auth)
                    && can_shallow_clone(url, reference);

                if !can_perform_full_clone {
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
                let (mut fetch_options, data) =
                    self.fetch_options_for(url, reference, &config, Some(data));
                // Here we always want to perform full clone, so set depth to 0.
                fetch_options.depth(0);
                builder.fetch_options(fetch_options);
                builder.clone(url.as_str(), destination).map_err(|e| {
                    trace!(data = ?data.borrow(), "callback data after full clone");
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
            let (callbacks, data) = callbacks(ctx, config, &url, None);
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
