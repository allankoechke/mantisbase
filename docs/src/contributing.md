# Contributing

Run formatting, clippy, and tests before opening a PR:

```bash
cargo fmt --check
cargo clippy --all-targets -- -D warnings
cargo test
```

Rust API documentation:

```bash
cargo doc --no-deps --open
```

Markdown book (requires [mdBook](https://github.com/rust-lang/mdBook)):

```bash
cd docs && mdbook build
```
