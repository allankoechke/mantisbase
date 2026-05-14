use mantisbase::http::openapi::build_openapi_value;
use mantisbase::storage::{LibsqlStore, Store};

#[tokio::test]
async fn libsql_migrations_and_openapi_smoke() {
    let dir = tempfile::tempdir().unwrap();
    let mig = dir.path().join("migrations");
    std::fs::create_dir_all(&mig).unwrap();
    let store = Store::Libsql(
        LibsqlStore::open_local(dir.path().to_str().unwrap(), "", &mig)
            .await
            .unwrap(),
    );
    let entities = store.list_entities_catalog().await.unwrap();
    let spec = build_openapi_value(&entities);
    assert_eq!(spec["openapi"], "3.0.3");
    assert!(spec.get("paths").is_some());
}
