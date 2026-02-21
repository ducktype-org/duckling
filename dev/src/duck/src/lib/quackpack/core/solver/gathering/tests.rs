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
    let manifest = dir.path().join("x");
    manifest.touch().unwrap();
    manifest.write(contents).unwrap();
    (dir, manifest)
}

async fn create_mock_server() -> MockServer {
    let server = MockServer::start().await;

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
fn test() {
    run_tokio_test(private::not_pinned_registry);
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

    /// Simple minimal test, root depends on foo 1.0.0 or 2.0.0, foo 1.0.0 depends on bar 3.0.0 or 4.0.0
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
}
