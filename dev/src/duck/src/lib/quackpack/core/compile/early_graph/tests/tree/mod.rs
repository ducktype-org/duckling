pub mod setup;
use std::collections::{HashMap, HashSet};

use setup::*;

use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::early_graph::{DependencyNode, EarlyGraph};
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::util::to_url::ToUrl;

#[test]
fn creates_valid_initial_graph() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        jobs: 1,
    };
    let graph = EarlyGraph::new_early(&bcx).unwrap();
    assert_eq!(graph.graph.root, identity_for("root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                identity_for("root"),
                DependencyNode::new(vec![fetcher_identity_for("foo")])
            ),
            (
                fetcher_identity_for("foo"),
                DependencyNode::new(vec![fetcher_identity_for("bar")])
            ),
            (
                fetcher_identity_for("bar"),
                DependencyNode::new(vec![fetcher_identity_for("baz")])
            ),
            (fetcher_identity_for("baz"), DependencyNode::new(vec![])),
        ])
    );
}

#[test]
fn expands_valid_features1() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["use_bar".into()],
        profile,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    let root_features = graph
        .package(&identity_for("root"))
        .enabled_features()
        .clone();

    let foo_features = graph
        .package(&fetcher_identity_for("foo"))
        .enabled_features()
        .clone();

    let bar_features = graph
        .package(&fetcher_identity_for("bar"))
        .enabled_features()
        .clone();

    let baz_features = graph
        .package(&fetcher_identity_for("baz"))
        .enabled_features()
        .clone();
    assert_eq!(root_features, HashSet::from(["use_bar".into()]));
    assert_eq!(foo_features, HashSet::from(["use_bar".into()]));
    assert!(bar_features.is_empty());
    assert!(baz_features.is_empty());
}

#[test]
fn expands_valid_features2() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    let root_features = graph
        .package(&identity_for("root"))
        .enabled_features()
        .clone();

    let foo_features = graph
        .package(&fetcher_identity_for("foo"))
        .enabled_features()
        .clone();

    let bar_features = graph
        .package(&fetcher_identity_for("bar"))
        .enabled_features()
        .clone();

    let baz_features = graph
        .package(&fetcher_identity_for("baz"))
        .enabled_features()
        .clone();
    assert_eq!(
        root_features,
        HashSet::from(["use_bar".into(), "full".into()])
    );
    assert_eq!(
        foo_features,
        HashSet::from(["use_bar".into(), "use_bar_with_baz".into()])
    );
    assert_eq!(bar_features, HashSet::from(["use_baz".into()]));
    assert!(baz_features.is_empty());
}

#[test]
fn expands_valid_features3() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["baz_without_bar".into()],
        profile,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    let root_features = graph
        .package(&identity_for("root"))
        .enabled_features()
        .clone();

    let foo_features = graph
        .package(&fetcher_identity_for("foo"))
        .enabled_features()
        .clone();

    let bar_features = graph
        .package(&fetcher_identity_for("bar"))
        .enabled_features()
        .clone();

    let baz_features = graph
        .package(&fetcher_identity_for("baz"))
        .enabled_features()
        .clone();
    assert_eq!(root_features, HashSet::from(["baz_without_bar".into()]));
    assert_eq!(
        foo_features,
        HashSet::from(["use_bar".into(), "use_bar_with_baz".into()])
    );
    assert_eq!(bar_features, HashSet::from(["use_baz".into()]));
    assert!(baz_features.is_empty());
}

#[test]
fn errors_with_nonexistent_features() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["nonexistent".into()],
        profile,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx).unwrap();
    let err = graph.populate_features(&bcx.used_features).unwrap_err();
    assert_eq!(
        err.to_string(),
        "while expanding features of the direct dependency `foo`
there is no such feature as `nonexistent`"
    );
}

#[test]
fn removes_inactive_deps1() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.graph.root, identity_for("root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                identity_for("root"),
                DependencyNode::new(vec![fetcher_identity_for("foo")])
            ),
            (fetcher_identity_for("foo"), DependencyNode::new(vec![])),
        ])
    );

    // Check that graph is in sync with `PackagesSet`.
    assert_eq!(graph.packages.inner.len(), 2);
    assert!(graph.packages.inner.contains_key(&identity_for("root")));
    assert!(
        graph
            .packages
            .inner
            .contains_key(&fetcher_identity_for("foo"))
    );
}

#[test]
fn removes_inactive_deps2() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["use_bar".into()],
        profile,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.graph.root, identity_for("root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                identity_for("root"),
                DependencyNode::new(vec![fetcher_identity_for("foo")])
            ),
            (
                fetcher_identity_for("foo"),
                DependencyNode::new(vec![fetcher_identity_for("bar")])
            ),
            (fetcher_identity_for("bar"), DependencyNode::new(vec![])),
        ])
    );

    // Check that graph is in sync with `PackagesSet`.
    assert_eq!(graph.packages.inner.len(), 3);
    assert!(graph.packages.inner.contains_key(&identity_for("root")));
    assert!(
        graph
            .packages
            .inner
            .contains_key(&fetcher_identity_for("foo"))
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&fetcher_identity_for("bar"))
    );
}

#[test]
fn removes_inactive_deps3() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.graph.root, identity_for("root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                identity_for("root"),
                DependencyNode::new(vec![fetcher_identity_for("foo")])
            ),
            (
                fetcher_identity_for("foo"),
                DependencyNode::new(vec![fetcher_identity_for("bar")])
            ),
            (
                fetcher_identity_for("bar"),
                DependencyNode::new(vec![fetcher_identity_for("baz")])
            ),
            (fetcher_identity_for("baz"), DependencyNode::new(vec![])),
        ])
    );

    // Check that graph is in sync with `PackagesSet`.
    assert_eq!(graph.packages.inner.len(), 4);
    assert!(graph.packages.inner.contains_key(&identity_for("root")));
    assert!(
        graph
            .packages
            .inner
            .contains_key(&fetcher_identity_for("foo"))
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&fetcher_identity_for("bar"))
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&fetcher_identity_for("baz"))
    );
}

#[test]
fn removes_inactive_deps4() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["baz_without_bar".into()],
        profile,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.graph.root, identity_for("root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                identity_for("root"),
                DependencyNode::new(vec![fetcher_identity_for("foo")])
            ),
            (
                fetcher_identity_for("foo"),
                DependencyNode::new(vec![fetcher_identity_for("bar")])
            ),
            (
                fetcher_identity_for("bar"),
                DependencyNode::new(vec![fetcher_identity_for("baz")])
            ),
            (fetcher_identity_for("baz"), DependencyNode::new(vec![])),
        ])
    );

    // Check that graph is in sync with `PackagesSet`.
    assert_eq!(graph.packages.inner.len(), 4);
    assert!(graph.packages.inner.contains_key(&identity_for("root")));
    assert!(
        graph
            .packages
            .inner
            .contains_key(&fetcher_identity_for("foo"))
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&fetcher_identity_for("bar"))
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&fetcher_identity_for("baz"))
    );
}

#[test]
fn missing_direct_dep_in_freeze() {
    let (ctx, root) = setup_mock_storage();
    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze_without_direct_dep(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        jobs: 1,
    };
    let err = EarlyGraph::new_early(&bcx).unwrap_err();
    assert_eq!(
        err.to_string(),
        format!(
            "malformed freezefile: missing direct dependency `{}`",
            fetcher_identity_for("foo")
        )
    );
}

#[test]
fn missing_transitive_dep_in_freeze() {
    let (ctx, root) = setup_mock_storage();
    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze_without_transitive_dep(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        jobs: 1,
    };
    let err = EarlyGraph::new_early(&bcx).unwrap_err();
    assert_eq!(
        err.to_string(),
        format!(
            "malformed freezefile: missing transitive dependency `{}`",
            fetcher_identity_for("bar")
        ),
    );
}
