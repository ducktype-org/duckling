use std::path::PathBuf;

use percent_encoding::{AsciiSet, CONTROLS, utf8_percent_encode};
use serde_json::Value;

use crate::quackpack::core::fetcher::ducknest::create_get_request;
use crate::quackpack::core::fetcher::git::pseudo_git_client::PseudoGitClient;
use crate::quackpack::core::fetcher::http::HttpClient;
use crate::quackpack::core::fetcher::util::http::Response;
use crate::quackpack::core::fetcher::util::http::traits_extensions::ResponseExt;
use crate::quackpack::core::{
    GitReference, Manifest, PackageLoader, ParseMode, manifest, parse_schema,
};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_url::ToUrl;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId};

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

#[allow(dead_code)]
/// Client for performing requests to Gitlab repositories.
pub struct GitlabClient<'duck> {
    client: HttpClient<'duck>,
}

#[allow(dead_code)] 
impl<'duck> GitlabClient<'duck> {
    /// Create a new [`GitlabClient`] instance.
    pub fn new(ctx: &'duck DuckContext) -> Self {
        Self {
            client: HttpClient::new(ctx),
        }
    }

    /// Get the underlying [`DuckContext`].
    pub fn ctx(&self) -> &DuckContext {
        self.client.ctx()
    }

    /// Get the base url for Gitlab's API for the specific repository.
    pub fn get_api_url(url: InternedUrl) -> Option<InternedUrl> {
        let scheme = url.scheme();
        let domain = url.domain()?;
        // Check that this is a gitlab repo.
        if !domain.contains("gitlab") {
            return None;
        }
        // `path()` adds `/` in the beginning.
        let path: String = url.path().chars().skip(1).collect();
        let path_encoded = utf8_percent_encode(&path, PATH_ENCODE_SET);
        let Ok(new_url) = format!("{scheme}://{domain}/api/v4/projects/{path_encoded}/").to_url()
        else {
            return None;
        };
        Some(new_url.into())
    }
}

impl PseudoGitClient for GitlabClient<'_> {
    fn get_commit_hash(&self, url: InternedUrl, reference: GitReference) -> QuackResult<StrId> {
        match reference {
            GitReference::Default => {
                let request = create_get_request(&url)?;
                let response = self.client.request(request)?;
                let default_branch = get_default_branch_from_response(response)?;
                self.get_commit_hash(url, GitReference::Branch(default_branch))
            }
            GitReference::Tag(tag) => {
                let url = url.join("repository/tags/")?.join(&tag)?;
                let request = create_get_request(&url)?;
                let response = self.client.request(request)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
            GitReference::Branch(branch) => {
                let url = url.join("repository/branches/")?.join(&branch)?;
                let request = create_get_request(&url)?;
                let response = self.client.request(request)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
            GitReference::Rev(commit) => Ok(commit),
        }
    }

    fn download_manifest(&self, url: InternedUrl, commit: StrId) -> QuackResult<Manifest> {
        let url = url.join(&format!(
            "repository/files/{}/raw?ref={}",
            PackageLoader::MANIFEST_NAME,
            commit
        ))?;
        let request = create_get_request(&url)?;
        let response = self.client.request(request)?;
        let deserialized_manifest = String::from_utf8(response.into_body())?;
        let manifest_schema = parse_schema(&deserialized_manifest)?;
        let manifest = manifest::parse(
            &manifest_schema,
            &PathBuf::new(),
            ParseMode::Package,
            self.ctx(),
        )?;
        Ok(manifest)
    }
}

#[allow(dead_code)] 
/// Deserialize the response for requests `.../branches/<branch>` and `.../tags/<tag>` and get the `commit.id` field.
fn get_commit_from_response(response: Response) -> QuackResult<StrId> {
    let data: Value = response.deserialize_json()?;
    let commit_hash = data
        .get("commit")
        .and_then(|v| v.get("id"))
        .context("failed to find the commit hash in the response")?;
    let commit_hash = commit_hash
        .as_str()
        .context("failed to interpret the commit hash as string")?
        .into();
    Ok(commit_hash)
}

#[allow(dead_code)] 
/// Deserialize the response for request to the url of the repository and get the `default_branch` field.
fn get_default_branch_from_response(response: Response) -> QuackResult<StrId> {
    let data: Value = response.deserialize_json()?;
    let default_branch = data
        .get("default_branch")
        .context("failed to find the default branch in the response")?;
    let default_branch = default_branch
        .as_str()
        .context("failed to interpret the default branch as string")?
        .into();
    Ok(default_branch)
}

#[cfg(test)]
mod test {
    use crate::quackpack::core::fetcher::git::gitlab_client::GitlabClient;
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn base_api_url() {
        let repo_url = "https://gitlab.com/foo/xd".to_url().unwrap().into();
        let api_url = "https://gitlab.com/api/v4/projects/foo%2Fxd/"
            .to_url()
            .unwrap();
        assert_eq!(api_url, GitlabClient::get_api_url(repo_url).unwrap())
    }
}
