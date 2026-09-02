use std::path::Path;
use std::pin::Pin;

use crate::quackpack::core::fetcher::git::fast_path::GitFastPathExt;
use crate::quackpack::core::fetcher::git::fast_path::github_api_client::GithubApiClient;
use crate::quackpack::core::lints::warnings::Warnings;
use crate::quackpack::core::{
    GitReference, Manifest, PackageLoader, ParseMode, manifest, parse_schema,
};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::{QuackResult, StrId};

/// Client serving a git fast-path for Github repositories.
pub struct GithubClient<'duck> {
    client: &'duck GithubApiClient<'duck>,
    repo_api_url: InternedUrl,
}

impl<'duck> GithubClient<'duck> {
    /// Create new [`GithubClient`].
    pub fn new(client: &'duck GithubApiClient, repo_url: InternedUrl) -> Option<Self> {
        let repo_api_url = GithubApiClient::get_api_url(&repo_url)?;
        Some(Self {
            client,
            repo_api_url: repo_api_url.into(),
        })
    }
}

impl<'duck> GitFastPathExt for GithubClient<'duck> {
    fn get_commit_hash(
        &self,
        reference: GitReference,
    ) -> Pin<Box<dyn Future<Output = QuackResult<StrId>> + '_>> {
        Box::pin(async move {
            match reference {
                GitReference::Default => {
                    self.client
                        .retrieve_a_commit(&self.repo_api_url, None)
                        .await
                }
                GitReference::Tag(tag) => {
                    self.client
                        .retrieve_a_commit(&self.repo_api_url, Some(tag))
                        .await
                }
                GitReference::Branch(branch) => {
                    self.client
                        .retrieve_a_commit(&self.repo_api_url, Some(branch))
                        .await
                }
                // We also translate short commit ids to long ones.
                GitReference::Rev(commit) => {
                    self.client
                        .retrieve_a_commit(&self.repo_api_url, Some(commit))
                        .await
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
                Path::new(""), // Dummy path.
                ParseMode::Package,
                &mut warnings,
                self.client.ctx(),
            )?;
            Ok(manifest)
        })
    }
}
