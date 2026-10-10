// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

pub mod setup;
use std::collections::HashMap;

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
/// `root` removes `bar` (not-specified feature), therefore also `baz` should be removed.
fn removes_inactive_deps1() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
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
        HashMap::from_iter([(
            mock_local_identity(root.path(), "root").into(),
            DependencyNode::new(vec![])
        ),])
    );

    // Check that graph is in sync with `PackagesSet`.
    assert_eq!(graph.packages.inner.len(), 1);
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_local_identity(root.path(), "root").into())
    );
}

#[test]
/// Here both `bar` and `baz` should be active.
fn removes_inactive_deps2() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
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
        used_features: vec!["use-bar".into()],
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
            .contains_key(&mock_registry_identity("bar").into())
    );
    assert!(
        graph
            .packages
            .inner
            .contains_key(&mock_registry_identity("baz").into())
    );
}
