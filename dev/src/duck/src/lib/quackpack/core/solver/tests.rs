use std::{collections::HashMap, path::PathBuf};

use tempfile::{TempDir, tempdir};
use wiremock::{
    Mock, MockServer, ResponseTemplate,
    matchers::{method, path},
};

use crate::{
    DuckCtx,
    quackpack::{
        core::{Version, fetcher::types, git_access::GitAccess},
        schemas::registry,
    },
    util_common::path_ops_ext::PathOpsExt,
};

fn setup_duck_ctx() -> (DuckCtx, TempDir) {
    // We set cache directory to a temporary directory, so we can use `Fetcher` without
    // worrying about leaving traces of tests in FS.
    let dir = tempdir().unwrap();
    // SAFETY: Setup is single threaded, and `Env` in `DuckCtx`, copies all envs.
    unsafe {
        std::env::set_var("DUCK_CACHE_DIR", dir.path());
    }
    let ctx = DuckCtx::default();
    // SAFETY: Setup is single threaded, and `Env` in `DuckCtx`, copies all envs.
    unsafe {
        std::env::remove_var("DUCK_CACHE_DIR");
    }
    (ctx, dir)
}

fn run_tokio_test<F, R>(f: F)
where
    F: FnOnce(DuckCtx) -> R,
    R: Future<Output = ()>,
{
    let (ctx, _dir) = setup_duck_ctx();
    tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .unwrap()
        .block_on(f(ctx));
}

fn prepare_manifest(contents: &str) -> (TempDir, PathBuf) {
    let dir = tempdir().unwrap();
    let manifest = dir.path().join("quackconfig.yml");
    manifest.touch().unwrap();
    manifest.write(contents).unwrap();
    (dir, manifest)
}

async fn create_mock_server() -> MockServer {
    let server = MockServer::start().await;

    // Assets for not_pinned_registry test.
    let a1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "a".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let a2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "a".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: [("a".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    let b2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "b".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    // Assets for not_pinned_registry test.
    Mock::given(method("GET"))
        .and(path("/packages/a/1.0.0"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&a1))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/b/2.0.0"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&b2))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/a"))
        .respond_with(
            ResponseTemplate::new(200).set_body_json(&types::MultiMetadata {
                packages_metadata: vec![a1, a2],
            }),
        )
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/b"))
        .respond_with(
            ResponseTemplate::new(200).set_body_json(&types::MultiMetadata {
                packages_metadata: vec![b2],
            }),
        )
        .mount(&server)
        .await;

    server
}

struct MockGitAccess();
impl GitAccess for MockGitAccess {
    fn git_path(&self, _url: url::Url, _commit: crate::StrId) -> PathBuf {
        panic!("unimplemented")
    }

    fn is_stored(&self, _url: url::Url, _commit: crate::StrId) -> bool {
        panic!("unimplemented")
    }

    fn store(
        &mut self,
        _url: url::Url,
        _commit: crate::StrId,
        _source_path: &std::path::Path,
    ) -> crate::QuackResult<()> {
        panic!("unimplemented")
    }
}

#[test]
fn test_new_dependency() {
    run_tokio_test(private::new_dependency);
}

#[test]
fn test_unnecessary_dependency() {
    run_tokio_test(private::remove_unnecessary_dependency);
}

#[test]
fn test_no_longer_working_dependency() {
    run_tokio_test(private::no_longer_working_dependency);
}

mod private {
    use std::sync::Arc;

    use tokio::sync::Mutex;
    use url::Url;

    use crate::{
        QpCtx,
        quackpack::core::{
            PackageCtx, ShouldRunSolverEngine, Solver,
            fetcher::Fetcher,
            solver_freeze::{SolverFreeze, SolverPackageFreeze},
            solver_mode::SolverMode,
            types_common::{ExpandedLocation, ExpandedPackage, InternedExpandedLocation},
        },
    };

    use super::*;

    /// Tests a new dependency occuring in the manifest.
    /// Main package depends on *a* and *b*, but only *a* is present in the supplied freeze.
    pub async fn new_dependency(ctx: DuckCtx) {
        let server = create_mock_server().await;
        let qpctx = QpCtx::new(&ctx);
        let url: Url = server.uri().parse().unwrap();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let (_dir, manifest_path) = prepare_manifest(&format!(
            r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  a:
    source:
      registry_url: {}
    version: '1'
  b:
    source:
      registry_url: {}
    version: '2'
"#,
            &url, &url,
        ));
        let root_path = manifest_path.parent().unwrap().to_path_buf();
        let pkg_ctx = PackageCtx::new(root_path.clone(), &qpctx).unwrap();

        let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
            absolute_path: root_path.clone(),
        });
        let root_pkg = ExpandedPackage {
            location: loc_root,
            version: None,
        };

        let loc_a = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "a".into(),
        });
        let a_pkg = ExpandedPackage {
            location: loc_a,
            version: Some(1.into()),
        };

        let loc_b = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "b".into(),
        });
        let b_pkg = ExpandedPackage {
            location: loc_b,
            version: Some(2.into()),
        };

        let previous_freeze = SolverFreeze {
            main_pkg: root_pkg,
            package_freezes: [
                (
                    root_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [("a".into(), a_pkg)].into(),
                        features: [].into(),
                    },
                ),
                (
                    a_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: [].into(),
                    },
                ),
            ]
            .into(),
        };

        let solver = Solver::new(&pkg_ctx, &fetcher, previous_freeze, SolverMode::default());
        let ShouldRunSolverEngine::Yes(solver) = solver
            .prepare_solving(Arc::new(Mutex::new(MockGitAccess())))
            .await
            .unwrap()
        else {
            panic!()
        };
        let new_freeze = solver.solve().unwrap().new_freeze;
        assert!(new_freeze.main_pkg == root_pkg);
        assert!(
            new_freeze.package_freezes
                == [
                    (
                        root_pkg,
                        SolverPackageFreeze {
                            dependencies_realization: [("a".into(), a_pkg), ("b".into(), b_pkg)]
                                .into(),
                            features: [].into()
                        }
                    ),
                    (
                        a_pkg,
                        SolverPackageFreeze {
                            dependencies_realization: [].into(),
                            features: [].into()
                        }
                    ),
                    (
                        b_pkg,
                        SolverPackageFreeze {
                            dependencies_realization: [].into(),
                            features: [].into()
                        }
                    ),
                ]
                .into()
        )
    }

    /// Tests a dependency becoming unnecessary.
    /// Main package depends on *b*, but both *a* and *b* are present in the supplied freeze.
    pub async fn remove_unnecessary_dependency(ctx: DuckCtx) {
        let server = create_mock_server().await;
        let qpctx = QpCtx::new(&ctx);
        let url: Url = server.uri().parse().unwrap();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let (_dir, manifest_path) = prepare_manifest(&format!(
            r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  b:
    source:
      registry_url: {}
    version: '2'
"#,
            &url,
        ));
        let root_path = manifest_path.parent().unwrap().to_path_buf();
        let pkg_ctx = PackageCtx::new(root_path.clone(), &qpctx).unwrap();

        let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
            absolute_path: root_path.clone(),
        });
        let root_pkg = ExpandedPackage {
            location: loc_root,
            version: None,
        };

        let loc_a = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "a".into(),
        });
        let a_pkg = ExpandedPackage {
            location: loc_a,
            version: Some(1.into()),
        };

        let loc_b = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "b".into(),
        });
        let b_pkg = ExpandedPackage {
            location: loc_b,
            version: Some(2.into()),
        };

        let previous_freeze = SolverFreeze {
            main_pkg: root_pkg,
            package_freezes: [
                (
                    root_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [("a".into(), a_pkg), ("b".into(), b_pkg)].into(),
                        features: [].into(),
                    },
                ),
                (
                    a_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: [].into(),
                    },
                ),
                (
                    b_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: [].into(),
                    },
                ),
            ]
            .into(),
        };

        let solver = Solver::new(&pkg_ctx, &fetcher, previous_freeze, SolverMode::default());
        let ShouldRunSolverEngine::No(answer) = solver
            .prepare_solving(Arc::new(Mutex::new(MockGitAccess())))
            .await
            .unwrap()
        else {
            panic!()
        };
        assert!(answer.new_freeze.main_pkg == root_pkg);
        assert!(
            answer.new_freeze.package_freezes
                == [
                    (
                        root_pkg,
                        SolverPackageFreeze {
                            dependencies_realization: [("b".into(), b_pkg)].into(),
                            features: [].into()
                        }
                    ),
                    (
                        b_pkg,
                        SolverPackageFreeze {
                            dependencies_realization: [].into(),
                            features: [].into()
                        }
                    ),
                ]
                .into()
        )
    }

    /// Tests that when supplied freeze realization is not correct, a new, correct realization is chosen.
    /// Main package depends on *a* with feature *a*, supplied freeze has *a* in version 1.0.0,
    /// which does not have the requested feature.
    /// Only *a* in version 2.0.0 has that feature and should be chosen to the new freeze.
    ///
    /// Note:
    /// [`SolverMode::Merciful`] is used in this test.
    pub async fn no_longer_working_dependency(ctx: DuckCtx) {
        let server = create_mock_server().await;
        let qpctx = QpCtx::new(&ctx);
        let url: Url = server.uri().parse().unwrap();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let (_dir, manifest_path) = prepare_manifest(&format!(
            r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  a:
    source:
      registry_url: {}
    version: 1 or 2
    features: [a]
"#,
            &url,
        ));
        let root_path = manifest_path.parent().unwrap().to_path_buf();
        let pkg_ctx = PackageCtx::new(root_path.clone(), &qpctx).unwrap();

        let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
            absolute_path: root_path.clone(),
        });
        let root_pkg = ExpandedPackage {
            location: loc_root,
            version: None,
        };

        let loc_a = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "a".into(),
        });
        let a1_pkg = ExpandedPackage {
            location: loc_a,
            version: Some(1.into()),
        };
        let a2_pkg = ExpandedPackage {
            location: loc_a,
            version: Some(2.into()),
        };

        let previous_freeze = SolverFreeze {
            main_pkg: root_pkg,
            package_freezes: [
                (
                    root_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [("a".into(), a1_pkg)].into(),
                        features: [].into(),
                    },
                ),
                (
                    a1_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: [].into(),
                    },
                ),
            ]
            .into(),
        };

        let mode = SolverMode {
            supress_foreign_manifests_errors: true,
            offline: false,
            frozen: false,
        };
        let solver = Solver::new(&pkg_ctx, &fetcher, previous_freeze, mode);
        let ShouldRunSolverEngine::Yes(solver) = solver
            .prepare_solving(Arc::new(Mutex::new(MockGitAccess())))
            .await
            .unwrap()
        else {
            panic!()
        };
        let new_freeze = solver.solve().unwrap().new_freeze;
        assert!(new_freeze.main_pkg == root_pkg);
        assert!(
            new_freeze.package_freezes
                == [
                    (
                        root_pkg,
                        SolverPackageFreeze {
                            dependencies_realization: [("a".into(), a2_pkg)].into(),
                            features: [].into()
                        }
                    ),
                    (
                        a2_pkg,
                        SolverPackageFreeze {
                            dependencies_realization: [].into(),
                            features: ["a".into()].into()
                        }
                    ),
                ]
                .into()
        )
    }
}
