# Installation

```bash
cargo build --release
./target/release/mantisbase --help
```

Create an admin and start the server:

```bash
./target/release/mantisbase --data-dir ./data admins add you@example.com 'your-password'
./target/release/mantisbase --data-dir ./data serve
```
