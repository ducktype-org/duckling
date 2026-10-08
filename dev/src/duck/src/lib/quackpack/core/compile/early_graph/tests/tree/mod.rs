// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

pub mod setup;
use std::collections::{HashMap, HashSet};

use setup::*;

use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::early_graph::tests::{
    mock_local_identity, mock_local_pkg, mock_registry_identity, mock_registry_pkg,
};
use crate::quackpack::core::compile::early_graph::{DependencyNode, EarlyGraph};
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::storage::load_deps::load_packages_in_freeze as load_packages;
use crate::quackpack::core::storage::paths::Storage;

#[test]
fn creates_valid_initial_graph() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = EarlyGraph::new_early(&bcx, packages.pkgs).unwrap();
    assert_eq!(graph.graph.root, mock_local_identity(root.path(), "root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                mock_local_identity(root.path(), "root").into(),
                DependencyNode::new(vec![mock_registry_identity("foo").into()])
            ),
            (
                mock_registry_identity("foo").into(),
                DependencyNode::new(vec![mock_registry_identity("bar").into()])
            ),
            (
                mock_registry_identity("bar").into(),
                DependencyNode::new(vec![mock_registry_identity("baz").into()])
            ),
            (
                mock_registry_identity("baz").into(),
                DependencyNode::new(vec![])
            ),
        ])
    );
}

#[test]
fn expands_valid_features1() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["use_bar".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx, packages.pkgs).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    let root_features = graph
        .package(&mock_local_identity(root.path(), "root").into())
        .enabled_features()
        .clone();

    let foo_features = graph
        .package(&mock_registry_identity("foo").into())
        .enabled_features()
        .clone();

    let bar_features = graph
        .package(&mock_registry_identity("bar").into())
        .enabled_features()
        .clone();

    let baz_features = graph
        .package(&mock_registry_identity("baz").into())
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
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx, packages.pkgs).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    let root_features = graph
        .package(&mock_local_identity(root.path(), "root").into())
        .enabled_features()
        .clone();

    let foo_features = graph
        .package(&mock_registry_identity("foo").into())
        .enabled_features()
        .clone();

    let bar_features = graph
        .package(&mock_registry_identity("bar").into())
        .enabled_features()
        .clone();

    let baz_features = graph
        .package(&mock_registry_identity("baz").into())
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
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["baz_without_bar".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx, packages.pkgs).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    let root_features = graph
        .package(&mock_local_identity(root.path(), "root").into())
        .enabled_features()
        .clone();

    let foo_features = graph
        .package(&mock_registry_identity("foo").into())
        .enabled_features()
        .clone();

    let bar_features = graph
        .package(&mock_registry_identity("bar").into())
        .enabled_features()
        .clone();

    let baz_features = graph
        .package(&mock_registry_identity("baz").into())
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
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["nonexistent".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx, packages.pkgs).unwrap();
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
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        shared: false,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx, packages.pkgs).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.graph.root, mock_local_identity(root.path(), "root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                mock_local_identity(root.path(), "root").into(),
                DependencyNode::new(vec![mock_registry_identity("foo").into()])
            ),
            (
                mock_registry_identity("foo").into(),
                DependencyNode::new(vec![])
            ),
        ])
    );

    // Check that graph is in sync with `PackagesSet`.
    assert_eq!(graph.packages.inner.len(), 2);
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_local_identity(root.path(), "root").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("foo").into())
    );
}

#[test]
fn removes_inactive_deps2() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["use_bar".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx, packages.pkgs).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.graph.root, mock_local_identity(root.path(), "root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                mock_local_identity(root.path(), "root").into(),
                DependencyNode::new(vec![mock_registry_identity("foo").into()])
            ),
            (
                mock_registry_identity("foo").into(),
                DependencyNode::new(vec![mock_registry_identity("bar").into()])
            ),
            (
                mock_registry_identity("bar").into(),
                DependencyNode::new(vec![])
            ),
        ])
    );

    // Check that graph is in sync with `PackagesSet`.
    assert_eq!(graph.packages.inner.len(), 3);
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_local_identity(root.path(), "root").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("foo").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("bar").into())
    );
}

#[test]
fn removes_inactive_deps3() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx, packages.pkgs).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.graph.root, mock_local_identity(root.path(), "root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                mock_local_identity(root.path(), "root").into(),
                DependencyNode::new(vec![mock_registry_identity("foo").into()])
            ),
            (
                mock_registry_identity("foo").into(),
                DependencyNode::new(vec![mock_registry_identity("bar").into()])
            ),
            (
                mock_registry_identity("bar").into(),
                DependencyNode::new(vec![mock_registry_identity("baz").into()])
            ),
            (
                mock_registry_identity("baz").into(),
                DependencyNode::new(vec![])
            ),
        ])
    );

    // Check that graph is in sync with `PackagesSet`.
    assert_eq!(graph.packages.inner.len(), 4);
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_local_identity(root.path(), "root").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("foo").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("bar").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("baz").into())
    );
}

#[test]
fn removes_inactive_deps4() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["baz_without_bar".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let mut graph = EarlyGraph::new_early(&bcx, packages.pkgs).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.graph.root, mock_local_identity(root.path(), "root"));
    assert_eq!(
        graph.graph.graph,
        HashMap::from_iter([
            (
                mock_local_identity(root.path(), "root").into(),
                DependencyNode::new(vec![mock_registry_identity("foo").into()])
            ),
            (
                mock_registry_identity("foo").into(),
                DependencyNode::new(vec![mock_registry_identity("bar").into()])
            ),
            (
                mock_registry_identity("bar").into(),
                DependencyNode::new(vec![mock_registry_identity("baz").into()])
            ),
            (
                mock_registry_identity("baz").into(),
                DependencyNode::new(vec![])
            ),
        ])
    );

    // Check that graph is in sync with `PackagesSet`.
    assert_eq!(graph.packages.inner.len(), 4);
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_local_identity(root.path(), "root").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("foo").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("bar").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("baz").into())
    );
}
