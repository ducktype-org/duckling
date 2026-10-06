// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use super::outputs;
use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::early_graph::creating_graph::create_early_graph_from_bcx;
use crate::quackpack::core::compile::early_graph::tests::cycling::setup::*;
use crate::quackpack::core::compile::early_graph::tests::{
    mock_local_identity, mock_local_pkg, mock_registry_pkg,
};
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::{UnitGraph, lower_early_graph};
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::storage::load_deps::load_packages_in_freeze as load_packages;
use crate::quackpack::core::storage::paths::Storage;

#[test]
fn collects_packages() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_local_pkg(root.path(), "cycle"),
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
    let graph = create_early_graph_from_bcx(&bcx, packages.pkgs).unwrap();
    let unit_graph = lower_early_graph(graph);
    for unit in unit_graph.units_sorted_by_id() {
        let expected: &[&str] = match unit.package().name().as_str() {
            "root" => &["bar", "baz", "foo", "root"],
            "foo" => &["baz", "foo"],
            "bar" => &["bar", "baz"],
            "baz" => &["baz"],
            _ => unreachable!(),
        };
        assert_packages_names(unit, &unit_graph, expected);
    }
}

#[test]
fn collects_packages_cycle() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_local_pkg(root.path(), "cycle"),
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
        used_features: vec!["cycle".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx, packages.pkgs).unwrap();
    let unit_graph = lower_early_graph(graph);
    for unit in unit_graph.units_sorted_by_id() {
        let expected: &[&str] = match unit.package().name().as_str() {
            "root" | "cycle" => &["bar", "cycle", "foo", "root"],
            "foo" => &["foo"],
            "bar" => &["bar"],
            _ => unreachable!(),
        };
        assert_packages_names(unit, &unit_graph, expected);
    }
}

fn assert_packages_names(unit: &Unit, graph: &UnitGraph, expected: &[&str]) {
    let mut names = outputs::collect_packages(unit, graph)
        .unwrap()
        .into_iter()
        .map(|package| package.import_name)
        .collect::<Vec<_>>();
    names.sort();
    assert_eq!(names, expected)
}
