// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for git fast path.
//! This means using outside knowledge about some git servers to perform necessary operation
//! without performing costly repository clones.
use std::pin::Pin;
use std::sync::Arc;

use crate::quackpack::core::fetcher::git::fast_path::github_api_client::GithubApiClient;
use crate::quackpack::core::fetcher::git::fast_path::github_client::GithubClient;
use crate::quackpack::core::fetcher::git::fast_path::gitlab_api_client::GitlabApiClient;
use crate::quackpack::core::fetcher::git::fast_path::gitlab_client::GitlabClient;
use crate::quackpack::core::fetcher::http_async::AsyncHttpClient;
use crate::quackpack::core::{GitReference, Manifest};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::{QuackResult, StrId};

mod github_api_client;
mod github_client;
mod gitlab_api_client;
mod gitlab_client;

/// Trait for comunicating with git repository servers which provide special APIs,
/// allowing us to postpone/completely omit clones.
pub trait GitFastPathExt {
    /// Translate a git reference into a commit hash.
    fn get_commit_hash(
        &self,
        reference: GitReference,
    ) -> Pin<Box<dyn Future<Output = QuackResult<StrId>> + '_>>;

    /// Download the manifest from a repository.
    fn download_manifest(
        &self,
        commit: StrId,
    ) -> Pin<Box<dyn Future<Output = QuackResult<Manifest>> + '_>>;
}

#[derive(Debug)]
/// Main entry point to the git fast path.
/// Used to create [`GitlabClient`] and [`GithubClient`] instances tailored to specific repositories.
pub struct GitFastPathClient<'duck> {
    gitlab: GitlabApiClient<'duck>,
    github: GithubApiClient<'duck>,
}

impl<'duck> GitFastPathClient<'duck> {
    /// Create new [`GitFastPathClient`].
    pub fn new(client: Arc<AsyncHttpClient<'duck>>) -> Self {
        Self {
            github: GithubApiClient::new(client.clone()),
            gitlab: GitlabApiClient::new(client),
        }
    }

    /// Try to get [`GitFastPathExt`] instance, tailored to the given repository.
    pub fn try_get_client(
        &'duck self,
        repo_url: InternedUrl,
    ) -> Option<Box<dyn GitFastPathExt + 'duck>> {
        if let Some(client) = GithubClient::new(&self.github, repo_url) {
            Some(Box::new(client))
        } else if let Some(client) = GitlabClient::new(&self.gitlab, repo_url) {
            Some(Box::new(client))
        } else {
            None
        }
    }
}
