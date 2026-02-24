mod setup;
use std::collections::HashSet;

use setup::*;

use crate::{
    QpCtx,
    quackpack::core::{
        PackageLoader,
        compile::{
            BuildContext,
            compiler_graph::{CompilerGraph, DependencyNode},
        },
        storage::paths::Storage,
    },
};

#[test]
fn creates_valid_initial_graph() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec![],
        profile: "debug".into(),
    };
    let graph = CompilerGraph::new_early(&bctx).unwrap();
    assert_eq!(
        graph.graph.root,
        DependencyNode {
            node: "root 1.0.0".parse().unwrap(),
            dependencies: vec![DependencyNode {
                node: "foo 1.0.0".parse().unwrap(),
                dependencies: vec![DependencyNode {
                    node: "bar 1.0.0".parse().unwrap(),
                    dependencies: vec![DependencyNode {
                        node: "baz 1.0.0".parse().unwrap(),
                        dependencies: vec![]
                    }]
                }]
            },],
        }
    );
    let order = graph
        .graph
        .determine_compilation_order()
        .unwrap()
        .iter()
        .map(|node| node.node.to_string())
        .collect::<Vec<_>>();
    assert_eq!(order, ["baz 1.0.0", "bar 1.0.0", "foo 1.0.0", "root 1.0.0"]);
}

#[test]
fn expands_valid_features1() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["use_bar".into()],
        profile: "debug".into(),
    };
    let graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    let root_features = graph
        .package("root 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
        .enabled_features()
        .clone();

    let foo_features = graph
        .package("foo 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
        .enabled_features()
        .clone();

    let bar_features = graph
        .package("bar 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
        .enabled_features()
        .clone();

    let baz_features = graph
        .package("baz 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
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
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["full".into()],
        profile: "debug".into(),
    };
    let graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    let root_features = graph
        .package("root 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
        .enabled_features()
        .clone();

    let foo_features = graph
        .package("foo 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
        .enabled_features()
        .clone();

    let bar_features = graph
        .package("bar 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
        .enabled_features()
        .clone();

    let baz_features = graph
        .package("baz 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
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
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["baz_without_bar".into()],
        profile: "debug".into(),
    };
    let graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    let root_features = graph
        .package("root 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
        .enabled_features()
        .clone();

    let foo_features = graph
        .package("foo 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
        .enabled_features()
        .clone();

    let bar_features = graph
        .package("bar 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
        .enabled_features()
        .clone();

    let baz_features = graph
        .package("baz 1.0.0".parse().unwrap())
        .unwrap()
        .read()
        .unwrap()
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
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["nonexistent".into()],
        profile: "debug".into(),
    };
    let graph = CompilerGraph::new_early(&bctx).unwrap();
    let err = graph.populate_features(&bctx.used_features).unwrap_err();
    assert_eq!(
        err.to_string(),
        "while expanding features of the direct dependency `foo`
there is no such feature as `nonexistent`"
    );
}

#[test]
fn removes_inactive_deps1() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec![],
        profile: "debug".into(),
    };
    let mut graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    graph.remove_disabled_dependencies().unwrap();
    assert_eq!(
        graph.graph.root,
        DependencyNode {
            node: "root 1.0.0".parse().unwrap(),
            dependencies: vec![DependencyNode {
                node: "foo 1.0.0".parse().unwrap(),
                dependencies: vec![],
            },],
        }
    );
}

#[test]
fn removes_inactive_deps2() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["use_bar".into()],
        profile: "debug".into(),
    };
    let mut graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    graph.remove_disabled_dependencies().unwrap();
    assert_eq!(
        graph.graph.root,
        DependencyNode {
            node: "root 1.0.0".parse().unwrap(),
            dependencies: vec![DependencyNode {
                node: "foo 1.0.0".parse().unwrap(),
                dependencies: vec![DependencyNode {
                    node: "bar 1.0.0".parse().unwrap(),
                    dependencies: vec![],
                }]
            },],
        }
    );
}

#[test]
fn removes_inactive_deps3() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["full".into()],
        profile: "debug".into(),
    };
    let mut graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    graph.remove_disabled_dependencies().unwrap();
    assert_eq!(
        graph.graph.root,
        DependencyNode {
            node: "root 1.0.0".parse().unwrap(),
            dependencies: vec![DependencyNode {
                node: "foo 1.0.0".parse().unwrap(),
                dependencies: vec![DependencyNode {
                    node: "bar 1.0.0".parse().unwrap(),
                    dependencies: vec![DependencyNode {
                        node: "baz 1.0.0".parse().unwrap(),
                        dependencies: vec![]
                    }]
                }]
            },],
        }
    );
}

#[test]
fn removes_inactive_deps4() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["baz_without_bar".into()],
        profile: "debug".into(),
    };
    let mut graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    graph.remove_disabled_dependencies().unwrap();
    assert_eq!(
        graph.graph.root,
        DependencyNode {
            node: "root 1.0.0".parse().unwrap(),
            dependencies: vec![DependencyNode {
                node: "foo 1.0.0".parse().unwrap(),
                dependencies: vec![DependencyNode {
                    node: "bar 1.0.0".parse().unwrap(),
                    dependencies: vec![DependencyNode {
                        node: "baz 1.0.0".parse().unwrap(),
                        dependencies: vec![]
                    }]
                }]
            },],
        }
    );
}

#[test]
fn compilation_works1() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec![],
        profile: "debug".into(),
    };
    let mut graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    graph.remove_disabled_dependencies().unwrap();
    let compiler = MockCompiler::default();
    graph.compile(&compiler, &bctx).unwrap();
    let compilations: &[Compilation] = &compiler.compilations.lock().unwrap();
    assert_eq!(
        compilations,
        [
            Compilation {
                root: "foo".into(),
                deps: vec![]
            },
            Compilation {
                root: "root".into(),
                deps: vec!["foo".into()]
            }
        ]
    );
}

#[test]
fn compilation_works2() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["use_bar".into()],
        profile: "debug".into(),
    };
    let mut graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    graph.remove_disabled_dependencies().unwrap();
    let compiler = MockCompiler::default();
    graph.compile(&compiler, &bctx).unwrap();
    let compilations: &[Compilation] = &compiler.compilations.lock().unwrap();
    assert_eq!(
        compilations,
        [
            Compilation {
                root: "bar".into(),
                deps: vec![]
            },
            Compilation {
                root: "foo".into(),
                deps: vec!["bar".into()]
            },
            Compilation {
                root: "root".into(),
                deps: vec!["foo".into()]
            }
        ]
    );
}

#[test]
fn compilation_works3() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["full".into()],
        profile: "debug".into(),
    };
    let mut graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    graph.remove_disabled_dependencies().unwrap();
    let compiler = MockCompiler::default();
    graph.compile(&compiler, &bctx).unwrap();
    let compilations: &[Compilation] = &compiler.compilations.lock().unwrap();
    assert_eq!(
        compilations,
        [
            Compilation {
                root: "baz".into(),
                deps: vec![]
            },
            Compilation {
                root: "bar".into(),
                deps: vec!["baz".into()]
            },
            Compilation {
                root: "foo".into(),
                deps: vec!["bar".into()]
            },
            Compilation {
                root: "root".into(),
                deps: vec!["foo".into()]
            }
        ]
    );
}

#[test]
fn compilation_works4() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec!["baz_without_bar".into()],
        profile: "debug".into(),
    };
    let mut graph = CompilerGraph::new_early(&bctx).unwrap();
    graph.populate_features(&bctx.used_features).unwrap();
    graph.remove_disabled_dependencies().unwrap();
    let compiler = MockCompiler::default();
    graph.compile(&compiler, &bctx).unwrap();
    let compilations: &[Compilation] = &compiler.compilations.lock().unwrap();
    assert_eq!(
        compilations,
        [
            Compilation {
                root: "baz".into(),
                deps: vec![]
            },
            Compilation {
                root: "bar".into(),
                deps: vec!["baz".into()]
            },
            Compilation {
                root: "foo".into(),
                deps: vec!["bar".into()]
            },
            Compilation {
                root: "root".into(),
                deps: vec!["foo".into()]
            }
        ]
    );
}

#[test]
fn cycle_in_freeze() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze_with_cycle(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec![],
        profile: "debug".into(),
    };
    let err = CompilerGraph::new_early(&bctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        "malformed freezefile: cycle `root 1.0.0` -> `foo 1.0.0` -> `bar 1.0.0` -> `foo 1.0.0`"
    );
}

#[test]
fn missing_direct_dep_in_freeze() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze_without_direct_dep(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec![],
        profile: "debug".into(),
    };
    let err = CompilerGraph::new_early(&bctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        "malformed freezefile: missing direct dependency `foo 1.0.0`"
    );
}

#[test]
fn missing_transient_dep_in_freeze() {
    let (ctx, root) = setup_mock_storage();
    let qp_ctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_at_exact_directory(&root.path().join("root"), &qp_ctx).unwrap();
    let bctx = BuildContext {
        duck_ctx: &ctx,
        package: &package,
        freeze: freeze_without_transient_dep(),
        storage: Storage::new(ctx.duck_home()),
        used_features: vec![],
        profile: "debug".into(),
    };
    let err = CompilerGraph::new_early(&bctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        "malformed freezefile: missing transient dependency `bar 1.0.0`"
    );
}
