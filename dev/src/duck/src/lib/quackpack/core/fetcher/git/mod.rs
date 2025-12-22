//! Module responsible for interacting with Git repositories.

use std::path::Path;

use crate::{
    QpCtx, QuackResult, QuackResultContext, StrId,
    quackpack::{
        core::{BranchOrTag, Git, PackageLoader, fetcher::types::GitCloneResponse},
        util::async_helpers::{extract_single_item_from_vec, unpack_tokio_scoped_vector},
    },
};

use async_scoped::TokioScope;
use git2::{FetchOptions, Oid};
use git2::{Repository, build::RepoBuilder};

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// Client implementing interaction with Git repositories.
/// Currently it provides only static methods.
pub struct GitClient {}

impl GitClient {
    /// Clone a repository pointed by `source` into `destination`, and parse a package it contains.
    pub fn clone_blocking(
        source: &Git,
        destination: &Path,
        ctx: &QpCtx<'_>,
    ) -> QuackResult<GitCloneResponse> {
        let mut builder = RepoBuilder::new();

        if source.can_shallow_clone() {
            let mut fetch_options = FetchOptions::new();
            fetch_options.depth(1);
            builder.fetch_options(fetch_options);
        }
        // git2-rs doesn't support cloning with a given tag :(.
        if let BranchOrTag::Branch(branch) = source.branch_or_tag() {
            builder.branch(branch.as_str());
        }

        let repository = builder.clone(source.url().as_str(), destination)?;

        // Prefer specific commits over tags.
        if let Some(commit) = source.rev() {
            repository.checkout_commit(commit).with_context(|| {
                format!(
                    "when performing a checkout of a repository cloned from `{}` to a commit `{}`",
                    source.url(),
                    commit
                )
            })?;
        } else if let BranchOrTag::Tag(tag) = source.branch_or_tag() {
            repository.checkout_tag(tag).with_context(|| {
                format!(
                    "when performing a checkout of a repository cloned from `{}` to a tag `{}`",
                    source.url(),
                    tag
                )
            })?;
        }

        let commit = repository.head()?.peel_to_commit()?.id();
        let package = PackageLoader::find_at_exact_directory(destination, ctx)
            .with_context(|| {
                format!(
                    "git repository at `{}` is not a duckling package",
                    source.url()
                )
            })?
            .into_package();
        Ok(GitCloneResponse {
            commit_hash: commit.to_string().into(),
            package,
        })
    }

    /// Same as [`clone_blocking`](Self::clone_blocking), but wrapped in an async bloat.
    pub async fn clone_async(
        source: &Git,
        destination: &Path,
        ctx: &QpCtx<'_>,
    ) -> QuackResult<GitCloneResponse> {
        let (_, results) = TokioScope::scope_and_block(|spawner| {
            spawner.spawn_blocking(|| Self::clone_blocking(source, destination, ctx))
        });

        let results = unpack_tokio_scoped_vector(results)?;
        extract_single_item_from_vec(results)?
    }
}

trait RepositoryExt {
    /// Checkout `self` into a given commit.
    fn checkout_commit(&self, commit: StrId) -> QuackResult<()>;
    /// Checkout `self` into a given tag.
    fn checkout_tag(&self, tag: StrId) -> QuackResult<()>;
}

impl RepositoryExt for Repository {
    fn checkout_commit(&self, commit: StrId) -> QuackResult<()> {
        let oid =
            Oid::from_str(&commit).with_context(|| format!("`{commit}` is not a valid Oid"))?;
        let commit = self
            .find_commit(oid)
            .with_context(|| format!("repository does not have a commit `{}", commit))?;
        let tree = commit.tree()?;
        self.checkout_tree(tree.as_object(), None)?;
        self.set_head_detached(oid)?;
        Ok(())
    }

    fn checkout_tag(&self, tag: StrId) -> QuackResult<()> {
        let refname = format!("refs/tags/{}", tag);
        let reference = self
            .find_reference(&refname)
            .with_context(|| format!("repository does not have a tag `{}`", tag))?;

        let commit = reference.peel_to_commit()?;

        self.checkout_tree(commit.as_object(), None)?;

        self.set_head_detached(commit.id())?;
        Ok(())
    }
}
