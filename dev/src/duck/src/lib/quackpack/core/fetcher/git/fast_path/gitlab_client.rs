//! Module for using Gitlab's API instead of blindly cloning the full repository.
//! The whole documentation can be found here: <https://docs.gitlab.com/api/api_resources/>.
//!
//! Gitlab's API:
//! -------------
//! Assume we have a repository `foo` authored by `author`.
//! The dependency on such repository is described by a url `https://gitlab.com/author/foo`.
//! Then the base URL for getting information about this repository is `https://gitlab.com/api/v4/projects/author%2Ffoo`.
//! Let us call this `base_api_url`.
//!
//! We perform 3 types of queries.
//! 1. Get the name of the default branch: `base_api_url`.
//! 2. Get the commit hash for a given git reference: `base_api_url/repository/tags/<tag>` and `base_api_url/repository/heads/<branch_name>`.
//! 3. Download the manifest (for a given commit): `base_api_url/repository/files/<manifest_path>/raw?ref=<commit_hash>`.
#![expect(dead_code)]
use std::path::Path;

use percent_encoding::{AsciiSet, CONTROLS, utf8_percent_encode};
use serde::Deserialize;

use crate::quackpack::core::fetcher::git::fast_path::GitFastPathExt;
use crate::quackpack::core::fetcher::git::fast_path::gitlab_api_client::GitlabApiClient;
use crate::quackpack::core::fetcher::util::http::Response;
use crate::quackpack::core::fetcher::util::http::traits_extensions::ResponseExt;
use crate::quackpack::core::{
    GitReference, Manifest, PackageLoader, ParseMode, manifest, parse_schema,
};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_url::ToUrl;
use crate::{QuackResult, QuackResultContext, StrId};

/// Characters to encode in Gitlab servers' urls.
const PATH_ENCODE_SET: &AsciiSet = &CONTROLS
    .add(b' ')
    .add(b'"')
    .add(b'#')
    .add(b'<')
    .add(b'>')
    .add(b'?')
    .add(b'`')
    .add(b'{')
    .add(b'}')
    .add(b'/');

/// Client serving a git fast-path for Gitlab repositories.
pub struct GitlabClient<'duck> {
    client: &'duck GitlabApiClient<'duck>,
    repo_api_url: InternedUrl,
}

impl<'duck> GitFastPathExt<'duck> for GitlabClient<'duck> {
    type ApiClient = GitlabApiClient<'duck>;

    fn new(client: &'duck GitlabApiClient, repo_url: InternedUrl) -> Option<Self> {
        let user = repo_url.username();
        let port = repo_url.port();
        let scheme = repo_url.scheme();
        let domain = repo_url.domain()?;
        // Check that scheme is `http` or `https`.
        if scheme != "http" && scheme != "https" {
            return None;
        }
        // Check that this is a gitlab repo.
        if !domain.starts_with("gitlab") {
            return None;
        }
        // `path()` adds `/` in the beginning.
        let path: String = repo_url.path().chars().skip(1).collect();
        let path_encoded = utf8_percent_encode(&path, PATH_ENCODE_SET);
        let mut repo_api_url = format!("{scheme}://{domain}/api/v4/projects/{path_encoded}/")
            .to_url()
            .ok()?;
        repo_api_url.set_username(user).ok()?;
        repo_api_url.set_port(port).ok()?;
        Some(Self {
            client,
            repo_api_url: repo_api_url.into(),
        })
    }

    fn get_commit_hash(&self, reference: GitReference) -> QuackResult<StrId> {
        match reference {
            GitReference::Default => {
                let response = self.client.retrieve_project(&self.repo_api_url)?;
                let default_branch = get_default_branch_from_response(response)?;
                self.get_commit_hash(GitReference::Branch(default_branch))
            }
            GitReference::Tag(tag) => {
                let response = self.client.retrieve_a_commit(&self.repo_api_url, tag)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
            GitReference::Branch(branch) => {
                let response = self.client.retrieve_a_commit(&self.repo_api_url, branch)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
            GitReference::Rev(commit) => {
                // We also translate short commit ids to long ones.
                let response = self.client.retrieve_a_commit(&self.repo_api_url, commit)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
        }
    }

    fn download_manifest(&self, commit: StrId) -> QuackResult<Manifest> {
        let manifest_path = format!("{}/", PackageLoader::MANIFEST_NAME);
        let response =
            self.client
                .download_file_from_commit(&self.repo_api_url, &manifest_path, commit)?;
        let deserialized_manifest = String::from_utf8(response.into_body())?;
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

#[derive(Deserialize)]
/// Type representing the interesting part of the response to `/repository/commits/reference`.
/// Based on <https://docs.gitlab.com/api/commits/#retrieve-a-commit>.
struct CommitResponse {
    id: String,
}

/// Deserialize the response for requests `/repository/commits/reference` and get the `id` field.
fn get_commit_from_response(response: Response) -> QuackResult<StrId> {
    let data: CommitResponse = response
        .deserialize_json()
        .context("failed to deserialize response")?;
    Ok(data.id.into())
}

#[derive(Deserialize)]
/// Type representing the interesting part of the response to get repo request.
/// Based on <https://docs.gitlab.com/api/projects/#retrieve-a-project>.
struct DefaultBranchResponse {
    default_branch: String,
}

/// Deserialize the response for request to the url of the repository and get the `default_branch` field.
fn get_default_branch_from_response(response: Response) -> QuackResult<StrId> {
    let data: DefaultBranchResponse = response
        .deserialize_json()
        .context("failed to deserialize response")?;
    Ok(data.default_branch.into())
}

#[cfg(test)]
mod test {
    use crate::DuckContext;
    use crate::quackpack::core::fetcher::git::fast_path::GitFastPathExt;
    use crate::quackpack::core::fetcher::git::fast_path::gitlab_client::{
        GitlabApiClient, GitlabClient,
    };
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn base_api_url() {
        let ctx = DuckContext::default();
        let api_client = GitlabApiClient::new(&ctx);

        let repo_url = "https://gitlab.com/foo/xd".to_url().unwrap().into();
        let gitlab_client = GitlabClient::new(&api_client, repo_url).unwrap();
        let api_url = "https://gitlab.com/api/v4/projects/foo%2Fxd/"
            .to_url()
            .unwrap();
        assert_eq!(api_url, gitlab_client.repo_api_url)
    }
}
