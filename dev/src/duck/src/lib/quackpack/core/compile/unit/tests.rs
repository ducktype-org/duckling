// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::ops::ControlFlow;

use super::UnitType;
use super::graph::{CompilationInput, lower_early_graph};
use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::early_graph::creating_graph::create_early_graph_from_bcx;
use crate::quackpack::core::compile::early_graph::tests::cycling::setup::*;
use crate::quackpack::core::compile::early_graph::tests::{
    mock_local_identity, mock_local_pkg, mock_registry_identity, mock_registry_pkg,
};
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::graph::{GraphNodeId, UnitGraphNode};
use crate::quackpack::core::compile::unit::graph_visitor::GraphVisitor;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::storage::load_deps::load_packages_in_freeze as load_packages;
use crate::quackpack::core::storage::paths::Storage;

// **IMPORTANT**
// Some notes on the tests' structure:
// * we use [0u64; 0] to create an empty slice of u64; otherwise, there's also a serde_json's Value,
//   which can be compared against u64, and rustc complains about not-infering the type.

// **NOTE**
// To de-duplicate some code, we reuse setup from early_graph/ tests.

#[derive(Default)]
struct IdOrder(Vec<GraphNodeId>);

impl GraphVisitor for IdOrder {
    type Break = ();
    fn visit(&mut self, node: &UnitGraphNode) -> ControlFlow<Self::Break> {
        self.0.push(node.id());
        ControlFlow::Continue(())
    }
}

#[test]
fn lowers_early_graph() {
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
    let unit_graph = lower_early_graph(graph).unwrap();

    let bar_src_id = 0;
    let baz_src_id = 1;
    let foo_src_id = 2;
    let root_src_id = 3;
    let bar_dep_id = 4;
    let baz_dep_id = 5;
    let foo_dep_id = 6;
    let root_exe_id = 7;
    assert_eq!(unit_graph.compilation_order().len(), 8);

    assert!(unit_graph.root_id() == root_exe_id);

    let root_exe_unit = unit_graph.node_for(root_exe_id).as_unit().unwrap();
    assert_eq!(
        unit_graph.deps_for(root_exe_id),
        [
            bar_src_id,
            baz_src_id,
            foo_src_id,
            root_src_id,
            bar_dep_id,
            baz_dep_id,
            foo_dep_id
        ]
    );
    assert_eq!(root_exe_unit.identity(), root_identity);
    assert_eq!(root_exe_unit.unit_type(), UnitType::Binary);

    let foo_dep_unit = unit_graph.node_for(foo_dep_id).as_unit().unwrap();
    assert_eq!(unit_graph.deps_for(foo_dep_id), [baz_src_id, foo_src_id]);
    assert_eq!(foo_dep_unit.identity(), mock_registry_identity("foo"));
    assert_eq!(foo_dep_unit.unit_type(), UnitType::Dependency);

    let baz_dep_unit = unit_graph.node_for(baz_dep_id).as_unit().unwrap();
    assert_eq!(unit_graph.deps_for(baz_dep_id), [baz_src_id]);
    assert_eq!(baz_dep_unit.identity(), mock_registry_identity("baz"));
    assert_eq!(baz_dep_unit.unit_type(), UnitType::Dependency);

    let bar_dep_unit = unit_graph.node_for(bar_dep_id).as_unit().unwrap();
    assert_eq!(unit_graph.deps_for(bar_dep_id), [bar_src_id, baz_src_id]);
    assert_eq!(bar_dep_unit.identity(), mock_registry_identity("bar"));
    assert_eq!(bar_dep_unit.unit_type(), UnitType::Dependency);

    let root_src_input = unit_graph.node_for(root_src_id).as_input().unwrap();
    assert_eq!(
        *root_src_input,
        CompilationInput::PackageSourceCode(root_identity)
    );

    let foo_src_input = unit_graph.node_for(foo_src_id).as_input().unwrap();
    assert_eq!(
        *foo_src_input,
        CompilationInput::PackageSourceCode(mock_registry_identity("foo").as_identity())
    );

    let baz_src_input = unit_graph.node_for(baz_src_id).as_input().unwrap();
    assert_eq!(
        *baz_src_input,
        CompilationInput::PackageSourceCode(mock_registry_identity("baz").as_identity())
    );

    let bar_src_input = unit_graph.node_for(bar_src_id).as_input().unwrap();
    assert_eq!(
        *bar_src_input,
        CompilationInput::PackageSourceCode(mock_registry_identity("bar").as_identity())
    );
}

#[test]
fn lowers_early_graph_with_cycle() {
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
    let cycle_identity = mock_local_identity(root.path(), "cycle").as_identity();
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
    let unit_graph = lower_early_graph(graph).unwrap();

    let bar_src_id = 0;
    let cycle_src_id = 1;
    let foo_src_id = 2;
    let root_src_id = 3;
    let bar_dep_id = 4;
    let cycle_dep_id = 5;
    let foo_dep_id = 6;
    let root_exe_id = 7;
    assert_eq!(unit_graph.compilation_order().len(), 8);

    assert!(unit_graph.root_id() == root_exe_id);

    let root_exe_unit = unit_graph.node_for(root_exe_id).as_unit().unwrap();
    assert_eq!(
        unit_graph.deps_for(root_exe_id),
        [
            bar_src_id,
            cycle_src_id,
            foo_src_id,
            root_src_id,
            bar_dep_id,
            cycle_dep_id,
            foo_dep_id
        ]
    );
    assert_eq!(root_exe_unit.identity(), root_identity);
    assert_eq!(root_exe_unit.unit_type(), UnitType::Binary);

    let foo_dep_unit = unit_graph.node_for(foo_dep_id).as_unit().unwrap();
    assert_eq!(unit_graph.deps_for(foo_dep_id), [foo_src_id]);
    assert_eq!(foo_dep_unit.identity(), mock_registry_identity("foo"));
    assert_eq!(foo_dep_unit.unit_type(), UnitType::Dependency);

    let cycle_dep_unit = unit_graph.node_for(cycle_dep_id).as_unit().unwrap();
    assert_eq!(
        unit_graph.deps_for(cycle_dep_id),
        [bar_src_id, cycle_src_id, foo_src_id, root_src_id]
    );
    assert_eq!(cycle_dep_unit.identity(), cycle_identity);
    assert_eq!(cycle_dep_unit.unit_type(), UnitType::Dependency);

    let bar_dep_unit = unit_graph.node_for(bar_dep_id).as_unit().unwrap();
    assert_eq!(unit_graph.deps_for(bar_dep_id), [bar_src_id]);
    assert_eq!(bar_dep_unit.identity(), mock_registry_identity("bar"));
    assert_eq!(bar_dep_unit.unit_type(), UnitType::Dependency);

    let root_src_input = unit_graph.node_for(root_src_id).as_input().unwrap();
    assert_eq!(
        *root_src_input,
        CompilationInput::PackageSourceCode(root_identity)
    );

    let foo_src_input = unit_graph.node_for(foo_src_id).as_input().unwrap();
    assert_eq!(
        *foo_src_input,
        CompilationInput::PackageSourceCode(mock_registry_identity("foo").as_identity())
    );

    let cycle_src_input = unit_graph.node_for(cycle_src_id).as_input().unwrap();
    assert_eq!(
        *cycle_src_input,
        CompilationInput::PackageSourceCode(cycle_identity)
    );

    let bar_src_input = unit_graph.node_for(bar_src_id).as_input().unwrap();
    assert_eq!(
        *bar_src_input,
        CompilationInput::PackageSourceCode(mock_registry_identity("bar").as_identity())
    );
}

#[test]
fn basic_visitor_order() {
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
    let unit_graph = lower_early_graph(graph).unwrap();

    let bar_src_id = 0;
    let baz_src_id = 1;
    let foo_src_id = 2;
    let root_src_id = 3;
    let bar_dep_id = 4;
    let baz_dep_id = 5;
    let foo_dep_id = 6;
    let root_exe_id = 7;
    // Root depends on everything so the order is not that interesting...
    {
        let mut visitor = IdOrder::default();

        unit_graph
            .node_for(root_exe_id)
            .accept(&mut visitor, &unit_graph);

        assert_eq!(
            visitor.0,
            [
                root_exe_id,
                bar_src_id,
                baz_src_id,
                foo_src_id,
                root_src_id,
                bar_dep_id,
                baz_dep_id,
                foo_dep_id
            ]
        );
    }
}
