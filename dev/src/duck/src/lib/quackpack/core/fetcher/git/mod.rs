//! Module responsible for interacting with Git repositories.

use std::path::Path;

use crate::{
    QpCtx, QuackResult, QuackResultContext, StrId, qp_bail_internal,
    quackpack::core::{Git, GitRevision, PackageLoader, fetcher::types::GitCloneResponse},
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
        if let GitRevision::Branch(branch) = source.rev() {
            builder.branch(branch.as_str());
        }

        let repository = builder.clone(source.url().as_str(), destination)?;

        // Prefer specific commits over tags.
        if let Some(commit) = source.commit() {
            repository
                .checkout_to_a_given_commit(commit)
                .with_context(|| {
                    format!(
                        "when performing a checkout of a repository cloned from `{}` to a commit `{}`",
                        source.url(), commit
                    )
                })?;
        } else if let GitRevision::Tag(tag) = source.rev() {
            repository.checkout_to_a_given_tag(tag).with_context(|| {
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

        unpack_results_vec(results)
    }
}

/// Extract a single item stored in this vector.
fn extract_single<T>(vec: Vec<T>) -> Option<T> {
    let [single]: [T; 1] = vec.try_into().ok()?;
    Some(single)
}

/// Unpack a single result from a vec returned by [`TokioScope`](async_scoped::Scope).
fn unpack_results_vec<T>(
    vec: Vec<Result<QuackResult<T>, tokio::task::JoinError>>,
) -> QuackResult<T> {
    let Some(first) = extract_single(vec) else {
        qp_bail_internal!("we've given exactly one closure, we should have got exactly one result")
    };

    first.context_internal("git thread panicked")?
}

trait RepositoryExt {
    /// Checkout `self` into a given commit.
    fn checkout_to_a_given_commit(&self, commit: StrId) -> QuackResult<()>;
    /// Checkout `self` into a given tag.
    fn checkout_to_a_given_tag(&self, tag: StrId) -> QuackResult<()>;
}

impl RepositoryExt for Repository {
    fn checkout_to_a_given_commit(&self, commit: StrId) -> QuackResult<()> {
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

    fn checkout_to_a_given_tag(&self, tag: StrId) -> QuackResult<()> {
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
