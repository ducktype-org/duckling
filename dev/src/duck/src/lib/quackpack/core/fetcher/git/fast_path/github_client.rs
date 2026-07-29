use std::path::Path;

use crate::quackpack::core::fetcher::git::fast_path::GitFastPathExt;
use crate::quackpack::core::fetcher::git::fast_path::github_api_client::GithubApiClient;
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

impl<'duck> GitFastPathExt<'duck> for GithubClient<'duck> {
    type ApiClient = GithubApiClient<'duck>;

    fn new(client: &'duck GithubApiClient, repo_url: InternedUrl) -> Option<Self> {
        let repo_api_url = GithubApiClient::get_api_url(&repo_url)?;
        Some(Self {
            client,
            repo_api_url: repo_api_url.into(),
        })
    }

    fn get_commit_hash(&self, reference: GitReference) -> QuackResult<StrId> {
        match reference {
            GitReference::Default => self.client.retrieve_a_commit(&self.repo_api_url, None),
            GitReference::Tag(tag) => self.client.retrieve_a_commit(&self.repo_api_url, Some(tag)),
            GitReference::Branch(branch) => self
                .client
                .retrieve_a_commit(&self.repo_api_url, Some(branch)),
            // We also translate short commit ids to long ones.
            GitReference::Rev(commit) => self
                .client
                .retrieve_a_commit(&self.repo_api_url, Some(commit)),
        }
    }

    fn download_manifest(&self, commit: StrId) -> QuackResult<Manifest> {
        let deserialized_manifest = self.client.download_file_from_commit(
            &self.repo_api_url,
            PackageLoader::MANIFEST_NAME,
            commit,
        )?;
        let manifest_schema = parse_schema(&deserialized_manifest)?;
        let manifest = manifest::parse(
            &manifest_schema,
            Path::new(""), // Dummy path.
            ParseMode::Package,
            self.client.ctx(),
        )?;
        Ok(manifest)
    }
}
