use std::{collections::HashMap, path::PathBuf, time::Duration};

use tempfile::{TempDir, tempdir};
use wiremock::{
    Mock, MockServer, ResponseTemplate,
    matchers::{method, path},
};

use crate::{
    DuckCtx,
    quackpack::{
        core::{Version, fetcher::types, git_access::GitAccess},
        schemas::{
            OneEntryMap,
            registry::{self, DependencyCondition, DependencyFeature},
        },
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
    let manifest = dir.path().join("x");
    manifest.touch().unwrap();
    manifest.write(contents).unwrap();
    (dir, manifest)
}

async fn create_mock_server() -> MockServer {
    let server = MockServer::start().await;

    // Test 1
    let foo_bar_dep = registry::Dependency {
        version: vec![Version::new(3, 0, 0), Version::new(4, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.uri(),
            },
        },
        features: vec![],
        pinned: false,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        is_alias_for: None,
    };

    let foo1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: [("bar".into(), foo_bar_dep)].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let foo2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let bar3 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(3, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "bar".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let bar411 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(4, 1, 1),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "bar".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    // Test 2 & 3
    let dx_xd_dep = registry::Dependency {
        version: vec![Version::new(1, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.uri(),
            },
        },
        features: vec![DependencyFeature::Detailed(OneEntryMap {
            key: "dx".into(),
            value: DependencyCondition {
                package_features: Some(vec!["root".into()]),
            },
        })],
        pinned: true,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        is_alias_for: None,
    };

    let xd1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "xd".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: [("dx".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    let dx2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "dx".into(),
            description: "".into(),
        },
        dependencies: [("xd".into(), dx_xd_dep)].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: [("root".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    // Test 4
    let b_a_dep = registry::Dependency {
        version: vec![Version::new(1, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.uri(),
            },
        },
        features: vec![DependencyFeature::Simple("f".into())],
        pinned: true,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        is_alias_for: None,
    };

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
        features: [("f".into(), vec![])].into(),
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
        features: [].into(),
        profiles: HashMap::new(),
    };

    let b1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "b".into(),
            description: "".into(),
        },
        dependencies: [("a".into(), b_a_dep)].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: [].into(),
        profiles: HashMap::new(),
    };

    // Test 1
    Mock::given(method("GET"))
        .and(path("/packages/foo/1.0.0"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&foo1))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo/2.0.0"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&foo2))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo"))
        .respond_with(
            ResponseTemplate::new(200).set_body_json(&types::MultiMetadata {
                packages_metadata: vec![foo1, foo2],
            }),
        )
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/bar/3.0.0"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&bar3))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/bar/4.1.1"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&bar411))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/bar"))
        .respond_with(
            ResponseTemplate::new(200).set_body_json(&types::MultiMetadata {
                packages_metadata: vec![bar3, bar411],
            }),
        )
        .mount(&server)
        .await;

    // Test 2 & 3
    Mock::given(method("GET"))
        .and(path("/packages/xd/1.0.0"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&xd1))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/dx/2.0.0"))
        .respond_with(
            ResponseTemplate::new(200)
                .set_delay(Duration::from_secs(1))
                .set_body_json(&dx2),
        )
        .mount(&server)
        .await;

    // Test 4
    Mock::given(method("GET"))
        .and(path("/packages/a/1.0.0"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&a1))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/a"))
        .respond_with(
            ResponseTemplate::new(200)
                .set_delay(Duration::from_millis(200))
                .set_body_json(&types::MultiMetadata {
                    packages_metadata: vec![a1, a2],
                }),
        )
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/b"))
        .respond_with(
            ResponseTemplate::new(200)
                .set_delay(Duration::from_millis(100))
                .set_body_json(&types::MultiMetadata {
                    packages_metadata: vec![b1],
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
fn test_not_pinned_registry() {
    run_tokio_test(private::not_pinned_registry);
}

#[test]
fn test_pinned_registry() {
    run_tokio_test(private::pinned_registry);
}

#[test]
fn test_features() {
    run_tokio_test(private::features);
}

#[test]
fn test_pinned_request_while_pending_not_pinned() {
    run_tokio_test(private::pinned_request_while_pending_not_pinned);
}

mod private {
    use std::{collections::HashSet, sync::Arc};

    use tokio::sync::Mutex;
    use url::Url;

    use crate::{
        QpCtx,
        quackpack::core::{
            SolverMode,
            fetcher::Fetcher,
            gathering::gatherer::Gatherer,
            parse_manifest,
            types_common::{
                ExpandedLocation, ExpandedPackage, InternedExpandedLocation, InternedLocation,
                Location,
            },
        },
    };

    use super::*;

    /// Not pinned registry dependencies test.
    /// Synopsis:
    ///    * root depends on foo 1.0.0 or 2.0.0,
    ///    * foo 1.0.0 depends on bar 3.0.0 or 4.0.0
    pub async fn not_pinned_registry(ctx: DuckCtx) {
        let server = create_mock_server().await;
        let qpctx = QpCtx::new(&ctx);
        let url: Url = server.uri().parse().unwrap();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let (_dir, root_path) = prepare_manifest(&format!(
            r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  foo:
    source:
      registry_url: {}
    version: 1 or 2
"#,
            &url
        ));
        let root_manifest = parse_manifest(&root_path, &qpctx).unwrap();
        let git_access = Arc::new(Mutex::new(MockGitAccess()));
        let gatherer = Gatherer::new(&qpctx, &fetcher, git_access);
        let gathered_info = gatherer
            .explore(
                root_path.clone(),
                root_manifest.manifest().clone(),
                HashSet::new(),
                SolverMode::Strict,
            )
            .await
            .unwrap();
        let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
            absolute_path: root_path.clone(),
        });
        let loc_foo = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "foo".into(),
        });
        let loc_bar = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "bar".into(),
        });
        assert!(
            gathered_info.versions_for_location
                == HashMap::from([
                    (loc_root, HashSet::from([None])),
                    (
                        loc_foo,
                        HashSet::from([Some(Version::new(1, 0, 0)), Some(Version::new(2, 0, 0))])
                    ),
                    (
                        loc_bar,
                        HashSet::from([Some(Version::new(3, 0, 0)), Some(Version::new(4, 1, 1))])
                    ),
                ])
        );
        let packages = HashSet::from([
            ExpandedPackage {
                location: loc_root,
                version: None,
            },
            ExpandedPackage {
                location: loc_foo,
                version: Some(Version::new(1, 0, 0)),
            },
            ExpandedPackage {
                location: loc_foo,
                version: Some(Version::new(2, 0, 0)),
            },
            ExpandedPackage {
                location: loc_bar,
                version: Some(Version::new(3, 0, 0)),
            },
            ExpandedPackage {
                location: loc_bar,
                version: Some(Version::new(4, 1, 1)),
            },
        ]);
        assert!(
            gathered_info
                .gathered_manifests
                .keys()
                .copied()
                .collect::<HashSet<ExpandedPackage>>()
                == packages
        );
        assert!(
            gathered_info
                .possible_features
                .keys()
                .copied()
                .collect::<HashSet<ExpandedPackage>>()
                == packages
        );
        for (_, features) in gathered_info.possible_features {
            assert!(features == HashSet::new());
        }
        assert!(
            gathered_info.location_resolver
                == HashMap::from([
                    (
                        InternedLocation::new(Location::Local { path: root_path }),
                        loc_root
                    ),
                    (
                        InternedLocation::new(Location::Registry {
                            url: url.clone(),
                            real_name: "foo".into()
                        }),
                        loc_foo
                    ),
                    (
                        InternedLocation::new(Location::Registry {
                            url: url.clone(),
                            real_name: "bar".into()
                        }),
                        loc_bar
                    ),
                ])
        )
    }

    /// Pinned registry dependencies test.
    /// Synopsis:
    ///    * root depends on xd exactly 1.0.0,
    ///    * root depends on dx exactly 2.0.0,
    ///    * dx depends exactly on xd 1.0.0
    pub async fn pinned_registry(ctx: DuckCtx) {
        let server = create_mock_server().await;
        let qpctx = QpCtx::new(&ctx);
        let url: Url = server.uri().parse().unwrap();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let (_dir, root_path) = prepare_manifest(&format!(
            r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  xd:
    source:
      registry_url: {}
    version: '1'
    pinned: true
  dx:
    source:
      registry_url: {}
    version: '2'
    pinned: true
"#,
            &url, &url,
        ));
        let root_manifest = parse_manifest(&root_path, &qpctx).unwrap();
        let git_access = Arc::new(Mutex::new(MockGitAccess()));
        let gatherer = Gatherer::new(&qpctx, &fetcher, git_access);
        let gathered_info = gatherer
            .explore(
                root_path.clone(),
                root_manifest.manifest().clone(),
                HashSet::new(),
                SolverMode::Strict,
            )
            .await
            .unwrap();
        let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
            absolute_path: root_path.clone(),
        });
        let loc_xd = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "xd".into(),
        });
        let loc_dx = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "dx".into(),
        });
        assert!(
            gathered_info.versions_for_location
                == HashMap::from([
                    (loc_root, HashSet::from([None])),
                    (loc_xd, HashSet::from([Some(Version::new(1, 0, 0))])),
                    (loc_dx, HashSet::from([Some(Version::new(2, 0, 0))])),
                ])
        );
    }

    /// Features propagation test.
    /// Same scenario as in [`pinned_registry`], but additionally:
    ///    * root has feature *my_feature*,
    ///    * this feature forces dx to have feature *root*
    ///    * this feature forces xd to have feature *dx*
    pub async fn features(ctx: DuckCtx) {
        let server = create_mock_server().await;
        let qpctx = QpCtx::new(&ctx);
        let url: Url = server.uri().parse().unwrap();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let (_dir, root_path) = prepare_manifest(&format!(
            r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  xd:
    source:
      registry_url: {}
    version: '1'
    pinned: true
  dx:
    source:
      registry_url: {}
    features:
    - root:
        package_features: [my_feature] 
    version: '2'
    pinned: true

features:
  my_feature: []
"#,
            &url, &url,
        ));
        let root_manifest = parse_manifest(&root_path, &qpctx).unwrap();
        let git_access = Arc::new(Mutex::new(MockGitAccess()));
        let gatherer = Gatherer::new(&qpctx, &fetcher, git_access);
        let gathered_info = gatherer
            .explore(
                root_path.clone(),
                root_manifest.manifest().clone(),
                ["my_feature".into()].into(),
                SolverMode::Strict,
            )
            .await
            .unwrap();
        let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
            absolute_path: root_path.clone(),
        });
        let loc_xd = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "xd".into(),
        });
        let loc_dx = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "dx".into(),
        });
        assert!(
            gathered_info.possible_features
                == HashMap::from([
                    (
                        ExpandedPackage {
                            location: loc_root,
                            version: None,
                        },
                        ["my_feature".into()].into()
                    ),
                    (
                        ExpandedPackage {
                            location: loc_xd,
                            version: Some(Version::new(1, 0, 0)),
                        },
                        ["dx".into()].into()
                    ),
                    (
                        ExpandedPackage {
                            location: loc_dx,
                            version: Some(Version::new(2, 0, 0)),
                        },
                        ["root".into()].into()
                    ),
                ])
        );
    }

    pub async fn pinned_request_while_pending_not_pinned(ctx: DuckCtx) {
        let server = create_mock_server().await;
        let qpctx = QpCtx::new(&ctx);
        let url: Url = server.uri().parse().unwrap();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let (_dir, root_path) = prepare_manifest(&format!(
            r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  a:
    source:
      registry_url: {}
    version: 1 or 2
  b:
    source:
      registry_url: {}
    version: '1'
"#,
            &url, &url,
        ));
        let root_manifest = parse_manifest(&root_path, &qpctx).unwrap();
        let git_access = Arc::new(Mutex::new(MockGitAccess()));
        let gatherer = Gatherer::new(&qpctx, &fetcher, git_access);
        let gathered_info = gatherer
            .explore(
                root_path.clone(),
                root_manifest.manifest().clone(),
                [].into(),
                SolverMode::Strict,
            )
            .await
            .unwrap();
        let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
            absolute_path: root_path.clone(),
        });
        let loc_a = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "a".into(),
        });
        let loc_b = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: "b".into(),
        });
        assert!(
            gathered_info.versions_for_location
                == HashMap::from([
                    (loc_root, [None].into()),
                    (
                        loc_a,
                        [Some(Version::new(1, 0, 0)), Some(Version::new(2, 0, 0))].into()
                    ),
                    (loc_b, [Some(Version::new(1, 0, 0))].into())
                ])
        );
    }
}
