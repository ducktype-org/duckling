use std::collections::HashMap;

use tempfile::{TempDir, tempdir};
use wiremock::{
    Mock, MockServer, ResponseTemplate,
    matchers::{method, path},
};

use crate::quackpack::core::Version;

use super::*;

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

async fn create_mock_server() -> MockServer {
    let pkg1 = registry::Dependency {
        version: vec![Version::new(2, 3, 6)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: "https://google.com".into(),
            },
        },
        features: vec![],
        pinned: false,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        is_alias_for: None,
    };

    let pkg2 = registry::Dependency {
        version: vec![Version::new(2, 3, 4)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: "https://google.com".into(),
            },
        },
        features: vec![],
        pinned: false,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        is_alias_for: None,
    };

    let pkg3 = registry::Dependency {
        version: vec![Version::new(2, 4, 7)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: "https://google.com".into(),
            },
        },
        features: vec![],
        pinned: false,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        is_alias_for: None,
    };

    let bar_256 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 5, 6),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "bar".into(),
            description: "".into(),
        },
        dependencies: [("pkg1".into(), pkg1)].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let foo_123 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 2, 3),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: [("pkg2".into(), pkg2), ("pkg3".into(), pkg3)].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let foo_125 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 2, 5),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: registry::Dependencies::new(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let server = MockServer::start().await;

    // We explicitly disable those, so we can ensure, that we fetch them from a cache.
    // Mock::given(method("GET"))
    //     .and(path("/packages/bar/2.5.6"))
    //     .respond_with(ResponseTemplate::new(200).set_body_json(&bar_256))
    //     .mount(&server)
    //     .await;
    //
    Mock::given(method("GET"))
        .and(path("/packages/foo/1.2.5"))
        .respond_with(ResponseTemplate::new(404))
        .mount(&server)
        .await;
    //
    // Mock::given(method("GET"))
    //     .and(path("/packages/foo/1.2.3"))
    //     .respond_with(ResponseTemplate::new(200).set_body_json(&foo_123))
    //     .mount(&server)
    //     .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo"))
        .respond_with(
            ResponseTemplate::new(200).set_body_json(&types::MultiMetadata {
                packages_metadata: vec![foo_123, foo_125],
            }),
        )
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/bar"))
        .respond_with(
            ResponseTemplate::new(200).set_body_json(&types::MultiMetadata {
                packages_metadata: vec![bar_256],
            }),
        )
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/bar/2.5.6/download"))
        .respond_with(ResponseTemplate::new(200).set_body_string("bar-2.5.6"))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo/1.2.3/download"))
        .respond_with(ResponseTemplate::new(200).set_body_string("foo-1.2.3"))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo/1.2.5/download"))
        .respond_with(ResponseTemplate::new(200).set_body_string("foo-1.2.5"))
        .mount(&server)
        .await;

    server
}

#[test]
fn all_metadata_adds_to_cache() {
    run_tokio_test(private::all_metadata_adds_to_cache);
    run_tokio_test(private::without_cache_fetch_fails);
}

mod private {
    use super::*;
    pub async fn all_metadata_adds_to_cache(ctx: DuckCtx) {
        let server = create_mock_server().await;
        let fetcher = Fetcher::new(&ctx).unwrap();
        let response = fetcher
            .get_package_all_metadata(&server.uri().parse().unwrap(), "foo".into(), false)
            .await
            .unwrap();
        let FetcherResponse::Some(response) = response else {
            panic!("Offline response with offline flag not present");
        };
        assert_eq!(response.packages_metadata.len(), 2);
        let FetcherResponse::Some(fetched_from_cache) = fetcher
            .get_package_metadata(
                &types::PackageWithUrl {
                    id: "foo".into(),
                    version: Version::new(1, 2, 5),
                    url: server.uri().parse().unwrap(),
                },
                true,
            )
            .await
            .unwrap()
        else {
            panic!("Offline response when metadata should be present in cache");
        };
        assert_eq!(fetched_from_cache.metadata.name, "foo");
        assert_eq!(fetched_from_cache.metadata.version, Version::new(1, 2, 5));
    }

    pub async fn without_cache_fetch_fails(ctx: DuckCtx) {
        let server = create_mock_server().await;
        let fetcher = Fetcher::new(&ctx).unwrap();
        let err = fetcher
            .get_package_metadata(
                &types::PackageWithUrl {
                    id: "foo".into(),
                    version: Version::new(1, 2, 5),
                    url: server.uri().parse().unwrap(),
                },
                false,
            )
            .await
            .unwrap_err();
        assert_eq!(
            err.to_string(),
            format!(
                "while getting a metadata of `foo` version `1.2.5` from `{}/`
HTTP status client error (404 Not Found) for url ({}/packages/foo/1.2.5)",
                server.uri(),
                server.uri()
            )
        );
    }
}
