# Installation

```bash
cargo build --release
./target/release/mantisbase --help
```

Build the admin dashboard (Vite → `public/mb-dist/`, mounted at `/mb/`):

```bash
cd admin && npm install && npm run build && cd ..
```

Create an admin and start the server:

```bash
./target/release/mantisbase --data-dir ./data admins add you@example.com 'your-password'
./target/release/mantisbase --data-dir ./data serve
```
