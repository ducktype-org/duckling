use std::pin::Pin;

use crate::quackpack::core::fetcher::git::fast_path::GitFastPathExt;
use crate::quackpack::core::fetcher::git::fast_path::gitlab_api_client::GitlabApiClient;
use crate::quackpack::core::lints::warnings::Warnings;
use crate::quackpack::core::{
    GitReference, Manifest, PackageLoader, ParseMode, manifest, parse_schema,
};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::{QuackResult, StrId};

/// Client serving a git fast-path for Gitlab repositories.
pub struct GitlabClient<'duck> {
    client: &'duck GitlabApiClient<'duck>,
    repo_api_url: InternedUrl,
}

impl<'duck> GitlabClient<'duck> {
    /// Create new [`GitlabClient`].
    pub fn new(client: &'duck GitlabApiClient, repo_url: InternedUrl) -> Option<Self> {
        let repo_api_url = GitlabApiClient::get_api_url(&repo_url)?;
        Some(Self {
            client,
            repo_api_url: repo_api_url.into(),
        })
    }
}

impl<'duck> GitFastPathExt for GitlabClient<'duck> {
    fn get_commit_hash(
        &self,
        reference: GitReference,
    ) -> Pin<Box<dyn Future<Output = QuackResult<StrId>> + '_>> {
        Box::pin(async move {
            match reference {
                GitReference::Default => {
                    let default_branch = self.client.retrieve_project(&self.repo_api_url).await?;
                    Box::pin(self.get_commit_hash(GitReference::Branch(default_branch))).await
                }
                GitReference::Tag(tag) => {
                    let commit_hash = self
                        .client
                        .retrieve_a_commit(&self.repo_api_url, tag)
                        .await?;
                    Ok(commit_hash)
                }
                GitReference::Branch(branch) => {
                    let commit_hash = self
                        .client
                        .retrieve_a_commit(&self.repo_api_url, branch)
                        .await?;
                    Ok(commit_hash)
                }
                GitReference::Rev(commit) => {
                    // We also translate short commit ids to long ones.
                    let commit_hash = self
                        .client
                        .retrieve_a_commit(&self.repo_api_url, commit)
                        .await?;
                    Ok(commit_hash)
                }
            }
        })
    }

    fn download_manifest(
        &self,
        commit: StrId,
    ) -> Pin<Box<dyn Future<Output = QuackResult<Manifest>> + '_>> {
        Box::pin(async move {
            let deserialized_manifest = self
                .client
                .download_file_from_commit(&self.repo_api_url, PackageLoader::MANIFEST_NAME, commit)
                .await?;
            let mut warnings = Warnings::default();
            let manifest_schema = parse_schema(&deserialized_manifest, &mut warnings)?;
            let manifest = manifest::parse(
                &manifest_schema,
                ParseMode::GitFastPath,
                &mut warnings,
                self.client.ctx(),
            )?;
            Ok(manifest)
        })
    }
}
