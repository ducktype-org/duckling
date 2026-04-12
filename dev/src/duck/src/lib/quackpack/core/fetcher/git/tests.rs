use tempfile::{TempDir, tempdir};

use git2::{DescribeOptions, IndexAddOption, Repository, Signature};
use url::Url;

use crate::{
    DuckContext,
    quackpack::core::{BranchOrTag, Git, PackageLoader, fetcher::git::GitClient},
    util::path_ops_ext::PathOpsExt,
};

fn generate_local_git_repo() -> TempDir {
    let tmpdir = tempdir().unwrap();

    let repo = Repository::init(tmpdir.path()).unwrap();

    let test_file = tmpdir.path().join("test_file.txt");
    test_file.as_path().write("Hello, Git!").unwrap();
    let manifest = tmpdir.path().join(PackageLoader::MANIFEST_NAME);
    manifest
        .as_path()
        .write(
            "metadata:
  name: fixtured_git_dependency
  version: 1",
        )
        .unwrap();

    let mut index = repo.index().unwrap();
    index
        .add_all(
            ["test_file.txt", PackageLoader::MANIFEST_NAME],
            IndexAddOption::DEFAULT,
            None,
        )
        .unwrap();
    index.write().unwrap();

    let tree_id = index.write_tree().unwrap();
    let tree = repo.find_tree(tree_id).unwrap();
    let sig = Signature::now("name", "email").unwrap();
    let oid = repo
        .commit(Some("HEAD"), &sig, &sig, "Initial commit", &tree, &[])
        .unwrap();
    let commit = repo.find_commit(oid).unwrap();

    repo.branch("test-branch", &commit, true).unwrap();

    repo.tag("v1.0.0", commit.as_object(), &sig, "tag", false)
        .unwrap();
    tmpdir
}

#[test]
fn clone_local_repo() {
    let dir = generate_local_git_repo();

    let target = dir.path().join("cloned");
    let source = Git::new(
        Url::from_directory_path(dir.path()).unwrap(),
        BranchOrTag::Default,
        None,
    );

    let ctx = DuckContext::default();
    let _ = GitClient::clone_blocking(&source, &target, &ctx).unwrap();

    assert!(target.exists());
    assert!(target.is_dir());
    assert!(Repository::open(&target).is_ok());
    assert_eq!(
        target.join("test_file.txt").read_to_string().unwrap(),
        "Hello, Git!"
    );
}

#[test]
fn clone_local_repo_with_branch() {
    let dir = generate_local_git_repo();

    let target = dir.path().join("cloned");
    let source = Git::new(
        Url::from_directory_path(dir.path()).unwrap(),
        BranchOrTag::Branch("test-branch".into()),
        None,
    );

    let ctx = DuckContext::default();
    let _ = GitClient::clone_blocking(&source, &target, &ctx).unwrap();

    let repo = Repository::open(&target).unwrap();
    assert_eq!(repo.head().unwrap().shorthand().unwrap(), "test-branch");
}

#[test]
fn clone_local_repo_with_tag() {
    let dir = generate_local_git_repo();

    let target = dir.path().join("cloned");
    let source = Git::new(
        Url::from_directory_path(dir.path()).unwrap(),
        BranchOrTag::Tag("v1.0.0".into()),
        None,
    );

    let ctx = DuckContext::default();
    let _ = GitClient::clone_blocking(&source, &target, &ctx).unwrap();

    let repo = Repository::open(&target).unwrap();
    let mut opts = DescribeOptions::new();
    opts.describe_tags();

    assert_eq!(
        repo.describe(&opts).unwrap().format(None).unwrap(),
        "v1.0.0"
    );
}

#[test]
fn clone_local_repo_with_rev() {
    let dir = generate_local_git_repo();

    let repo = Repository::open(dir.path()).unwrap();
    let original_commit = repo.head().unwrap().peel_to_commit().unwrap();

    let target = dir.path().join("cloned");
    let source = Git::new(
        Url::from_directory_path(dir.path()).unwrap(),
        BranchOrTag::Default,
        Some(original_commit.id().to_string().into()),
    );
    let ctx = DuckContext::default();
    let _ = GitClient::clone_blocking(&source, &target, &ctx).unwrap();

    let repo = Repository::open(&target).unwrap();
    let new_commit = repo.head().unwrap().peel_to_commit().unwrap();

    assert_eq!(original_commit.id(), new_commit.id());
}
