mod setup;
use std::collections::{HashMap, HashSet};

use setup::*;

use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::early_dag::{DependencyNode, EarlyDag};
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::storage::paths::Storage;

#[test]
fn creates_valid_initial_graph() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        script_path: None,
    };
    let graph = EarlyDag::new_early(&bcx).unwrap();
    assert_eq!(graph.dag.root.to_string(), "root 1.0.0");
    assert_eq!(
        graph.dag.dag,
        HashMap::from_iter([
            (
                "root 1.0.0".parse().unwrap(),
                DependencyNode::new(vec![
                    "foo 1.0.0".parse().unwrap(),
                    "bar 1.0.0".parse().unwrap()
                ])
            ),
            (
                "foo 1.0.0".parse().unwrap(),
                DependencyNode::new(vec!["baz 1.0.0".parse().unwrap()])
            ),
            (
                "bar 1.0.0".parse().unwrap(),
                DependencyNode::new(vec!["baz 1.0.0".parse().unwrap()])
            ),
            ("baz 1.0.0".parse().unwrap(), DependencyNode::new(vec![])),
        ])
    );
}

#[test]
fn expands_valid_features1() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["use_foo_with_baz".into()],
        profile,
        script_path: None,
    };
    let mut graph = EarlyDag::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    let root_features = graph
        .package(&"root 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();

    let foo_features = graph
        .package(&"foo 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();

    let bar_features = graph
        .package(&"bar 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();

    let baz_features = graph
        .package(&"baz 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();
    assert_eq!(root_features, HashSet::from(["use_foo_with_baz".into()]));
    assert_eq!(foo_features, HashSet::from(["use_baz".into()]));
    assert!(bar_features.is_empty());
    assert!(baz_features.is_empty());
}

#[test]
fn expands_valid_features2() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["use_bar_with_baz".into()],
        profile,
        script_path: None,
    };
    let mut graph = EarlyDag::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    let root_features = graph
        .package(&"root 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();

    let foo_features = graph
        .package(&"foo 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();

    let bar_features = graph
        .package(&"bar 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();

    let baz_features = graph
        .package(&"baz 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();
    assert_eq!(root_features, HashSet::from(["use_bar_with_baz".into()]));
    assert!(foo_features.is_empty());
    assert_eq!(bar_features, HashSet::from(["use_baz".into()]));
    assert!(baz_features.is_empty());
}

#[test]
fn expands_valid_features3() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        script_path: None,
    };
    let mut graph = EarlyDag::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    let root_features = graph
        .package(&"root 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();

    let foo_features = graph
        .package(&"foo 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();

    let bar_features = graph
        .package(&"bar 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();

    let baz_features = graph
        .package(&"baz 1.0.0".parse().unwrap())
        .enabled_features()
        .clone();
    assert_eq!(
        root_features,
        HashSet::from([
            "use_foo_with_baz".into(),
            "full".into(),
            "use_bar_with_baz".into()
        ])
    );
    assert_eq!(foo_features, HashSet::from(["use_baz".into()]),);
    assert_eq!(bar_features, HashSet::from(["use_baz".into()]));
    assert!(baz_features.is_empty());
}

#[test]
fn errors_with_nonexistent_features() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["nonexistent".into()],
        profile,
        script_path: None,
    };
    let mut graph = EarlyDag::new_early(&bcx).unwrap();
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
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        script_path: None,
    };
    let mut graph = EarlyDag::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.dag.root.to_string(), "root 1.0.0");
    assert_eq!(
        graph.dag.dag,
        HashMap::from_iter([
            (
                "root 1.0.0".parse().unwrap(),
                DependencyNode::new(vec![
                    "foo 1.0.0".parse().unwrap(),
                    "bar 1.0.0".parse().unwrap()
                ])
            ),
            ("foo 1.0.0".parse().unwrap(), DependencyNode::new(vec![])),
            ("bar 1.0.0".parse().unwrap(), DependencyNode::new(vec![])),
            ("baz 1.0.0".parse().unwrap(), DependencyNode::new(vec![])),
        ])
    );
}

#[test]
fn removes_inactive_deps2() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["use_foo_with_baz".into()],
        profile,
        script_path: None,
    };
    let mut graph = EarlyDag::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.dag.root.to_string(), "root 1.0.0");
    assert_eq!(
        graph.dag.dag,
        HashMap::from_iter([
            (
                "root 1.0.0".parse().unwrap(),
                DependencyNode::new(vec![
                    "foo 1.0.0".parse().unwrap(),
                    "bar 1.0.0".parse().unwrap()
                ])
            ),
            (
                "foo 1.0.0".parse().unwrap(),
                DependencyNode::new(vec!["baz 1.0.0".parse().unwrap()])
            ),
            ("bar 1.0.0".parse().unwrap(), DependencyNode::new(vec![])),
            ("baz 1.0.0".parse().unwrap(), DependencyNode::new(vec![])),
        ])
    );
}

#[test]
fn removes_inactive_deps3() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["use_bar_with_baz".into()],
        profile,
        script_path: None,
    };
    let mut graph = EarlyDag::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.dag.root.to_string(), "root 1.0.0");
    assert_eq!(
        graph.dag.dag,
        HashMap::from_iter([
            (
                "root 1.0.0".parse().unwrap(),
                DependencyNode::new(vec![
                    "foo 1.0.0".parse().unwrap(),
                    "bar 1.0.0".parse().unwrap()
                ])
            ),
            ("foo 1.0.0".parse().unwrap(), DependencyNode::new(vec![])),
            (
                "bar 1.0.0".parse().unwrap(),
                DependencyNode::new(vec!["baz 1.0.0".parse().unwrap()])
            ),
            ("baz 1.0.0".parse().unwrap(), DependencyNode::new(vec![])),
        ])
    );
}

#[test]
fn removes_inactive_deps4() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        script_path: None,
    };
    let mut graph = EarlyDag::new_early(&bcx).unwrap();
    graph.populate_features(&bcx.used_features).unwrap();
    graph.remove_disabled_dependencies();
    assert_eq!(graph.dag.root.to_string(), "root 1.0.0");
    assert_eq!(
        graph.dag.dag,
        HashMap::from_iter([
            (
                "root 1.0.0".parse().unwrap(),
                DependencyNode::new(vec![
                    "foo 1.0.0".parse().unwrap(),
                    "bar 1.0.0".parse().unwrap()
                ])
            ),
            (
                "foo 1.0.0".parse().unwrap(),
                DependencyNode::new(vec!["baz 1.0.0".parse().unwrap()])
            ),
            (
                "bar 1.0.0".parse().unwrap(),
                DependencyNode::new(vec!["baz 1.0.0".parse().unwrap()])
            ),
            ("baz 1.0.0".parse().unwrap(), DependencyNode::new(vec![])),
        ])
    );
}

#[test]
fn cycle_in_freeze() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze_with_cycle(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        script_path: None,
    };
    let err = EarlyDag::new_early(&bcx).unwrap_err();
    assert_eq!(
        err.to_string(),
        "malformed freezefile: cycle `root 1.0.0` -> `foo 1.0.0` -> `bar 1.0.0` -> `foo 1.0.0`"
    );
}

#[test]
fn missing_direct_dep_in_freeze() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze_without_direct_dep(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        script_path: None,
    };
    let err = EarlyDag::new_early(&bcx).unwrap_err();
    assert_eq!(
        err.to_string(),
        "malformed freezefile: missing direct dependency `foo 1.0.0`"
    );
}

#[test]
fn missing_transitive_dep_in_freeze() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let bcx = BuildContext {
        pcx: &package,
        freeze: freeze_without_transitive_dep(),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec![],
        profile,
        script_path: None,
    };
    let err = EarlyDag::new_early(&bcx).unwrap_err();
    assert_eq!(
        err.to_string(),
        "malformed freezefile: missing transitive dependency `baz 1.0.0`"
    );
}
