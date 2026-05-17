//! Integration test for PostgreSQL storage. CI sets `TEST_PG_URL`.

#[tokio::test]
async fn postgres_migrate_and_admin_roundtrip() {
    let Some(url) = std::env::var("TEST_PG_URL")
        .ok()
        .filter(|s| !s.trim().is_empty())
    else {
        return;
    };

    use mantisbase::storage::PostgresStore;

    let mig = tempfile::tempdir().expect("migrations tempdir");
    let store = PostgresStore::connect(&url, mig.path())
        .await
        .expect("connect postgres");
    let email = format!("pgtest_{}@example.com", uuid::Uuid::new_v4());
    store
        .add_admin(&email, "hunter2hunter2")
        .await
        .expect("add_admin");
    let ok = store
        .verify_admin_basic(&email, "hunter2hunter2")
        .await
        .expect("verify");
    assert!(ok);
    let list = store.list_admins().await.expect("list");
    assert!(list.iter().any(|a| a.email == email));
    assert!(list.iter().find(|a| a.email == email).unwrap().active);
    assert!(
        !list
            .iter()
            .find(|a| a.email == email)
            .unwrap()
            .password_reset_required
    );
}
