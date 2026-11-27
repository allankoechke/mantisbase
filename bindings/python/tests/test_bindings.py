"""Exercise the installed mantisbase wheel.

CI runs this after repairing a wheel. It is not an import check: one temporary
server covers schema CRUD, SQL, auth, files, mail, and HTTP routes.

    python -m unittest discover -s bindings/python/tests -v
"""

import ast
import atexit
import http.client
import json
import os
import shutil
import socket
import tempfile
import time
import unittest
import urllib.parse

os.environ.setdefault("MB_DISABLE_RATE_LIMIT", "1")
os.environ.setdefault("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1")

import mantisbase

HOST = "127.0.0.1"
SECRET = "mantisbase-python-binding-test-secret"
STATE = {}

NOTES_SCHEMA = {
    "name": "notes",
    "type": "base",
    "fields": [
        {"name": "title", "type": "string", "required": True},
        {"name": "body", "type": "string"},
    ],
    "rules": {
        "list": {"mode": "public", "expr": ""},
        "get": {"mode": "public", "expr": ""},
        "add": {"mode": "public", "expr": ""},
        "update": {"mode": "public", "expr": ""},
        "delete": {"mode": "public", "expr": ""},
    },
}

LABELS_SCHEMA = {
    "name": "labels",
    "type": "base",
    "fields": [{"name": "name", "type": "string", "required": True}],
    "rules": {
        "list": {"mode": "public", "expr": ""},
        "get": {"mode": "public", "expr": ""},
        "add": {"mode": "public", "expr": ""},
        "update": {"mode": "public", "expr": ""},
        "delete": {"mode": "public", "expr": ""},
    },
}


def _free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((HOST, 0))
        return sock.getsockname()[1]


def _stop_app():
    app = STATE.get("app")
    if app is None:
        return
    STATE["app"] = None
    app.stop()


def _remove_workdir():
    root = STATE.get("root")
    if root and os.path.isdir(root):
        shutil.rmtree(root, ignore_errors=True)


atexit.register(_remove_workdir)
atexit.register(_stop_app)


def _register_routes(app):
    @app.router.get("/hello")
    def hello():
        return {"message": "Hello, World!"}

    @app.router.get("/notes/{id}")
    def read_note(req):
        note = app.entity("notes").read(req.path_param("id"))
        if note is None:
            return {"error": "missing"}
        return {"id": note["id"], "title": note["title"]}

    @app.router.get("/search")
    def search(req):
        return {"q": req.query_param("q"), "mark": req.header("X-Mark", "")}

    @app.router.post("/echo")
    def echo(req, res):
        res.json(201, {"got": req.json()})

    def block(req, res):
        res.json(401, {"error": "blocked"})
        return False

    @app.router.get("/blocked", middlewares=[block])
    def blocked():
        return {"ok": True}

    @app.router.get(
        "/admin-only", middlewares=[mantisbase.middlewares.require_admin()]
    )
    def admin_only():
        return {"ok": True}

    @app.router.get(
        "/whoami", middlewares=[mantisbase.middlewares.get_auth_token()]
    )
    def whoami(req):
        return {"token": req.auth_token()}

    @app.router.patch("/patched")
    def patched():
        return {"patched": True}

    app.router.add_route("PUT", "/replaced", lambda: {"replaced": True})
    app.router.redirect("/old", "/hello", status=301)


def _http(method, path, body=None, headers=None):
    connection = http.client.HTTPConnection(HOST, STATE["port"], timeout=5)
    try:
        connection.request(method, path, body=body, headers=headers or {})
        response = connection.getresponse()
        payload = response.read()
        header_map = {key.lower(): value for key, value in response.getheaders()}
        return response.status, header_map, payload
    finally:
        connection.close()


def _wait_until_ready():
    deadline = time.time() + 30
    last = None
    while time.time() < deadline:
        try:
            status, _, payload = _http("GET", "/api/v1/health")
            if status == 200 and b'"OK"' in payload:
                return
            last = (status, payload)
        except OSError as exc:
            last = exc
        time.sleep(0.1)
    raise RuntimeError(f"server did not accept connections: {last}")


def setUpModule():
    root = tempfile.mkdtemp(prefix="mantisbase-py-")
    STATE["root"] = root
    for name in ("data", "public", "scripts", "migrations", "templates"):
        os.makedirs(os.path.join(root, name), exist_ok=True)
    with open(os.path.join(root, "templates", "hello.txt"), "w", encoding="utf-8") as handle:
        handle.write("Hello {{name}}")

    port = _free_port()
    STATE["port"] = port
    app = mantisbase.App(
        host=HOST,
        port=port,
        data_dir=os.path.join(root, "data"),
        public_dir=os.path.join(root, "public"),
        scripts_dir=os.path.join(root, "scripts"),
        migrations_dir=os.path.join(root, "migrations"),
        templates_dir=os.path.join(root, "templates"),
        pool_size=2,
        dev=True,
        skip_admin_setup=True,
        secret_key=SECRET,
    )
    STATE["app"] = app
    _register_routes(app)
    created = app.create_schema(NOTES_SCHEMA)
    STATE["schema_id"] = created["id"]
    note = app.entity("notes").create({"title": "Hello from Python", "body": "seed"})
    STATE["note_id"] = note["id"]
    app.start(blocking=False)
    _wait_until_ready()


def tearDownModule():
    _stop_app()
    _remove_workdir()


class BindingsTest(unittest.TestCase):
    def setUp(self):
        self.app = STATE["app"]

    def test_module_surface(self):
        version = mantisbase.version()
        self.assertTrue(version)
        self.assertEqual(self.app.version, version)
        self.assertIs(mantisbase.Router, mantisbase.MbRouter)
        self.assertIs(mantisbase.Database, mantisbase.MbDatabase)
        self.assertIs(mantisbase.MantisRequest, mantisbase.MbRequest)
        self.assertIs(mantisbase.MantisResponse, mantisbase.MbResponse)
        self.assertTrue(issubclass(mantisbase.MantisError, Exception))
        stub = os.path.join(os.path.dirname(mantisbase.__file__), "mantisbase.pyi")
        self.assertTrue(os.path.isfile(stub), stub)
        with open(stub, encoding="utf-8") as handle:
            source = handle.read()
        ast.parse(source)
        self.assertIn("class App", source)

    def test_utils(self):
        self.assertTrue(mantisbase.utils.str_to_bool("yes"))
        self.assertFalse(mantisbase.utils.str_to_bool("no"))
        os.environ["MB_PY_BIND_TEST"] = "present"
        self.assertEqual(mantisbase.utils.get_env("MB_PY_BIND_TEST"), "present")
        self.assertEqual(mantisbase.utils.get_env("MB_PY_BIND_TEST_MISSING", "fallback"), "fallback")

        stored = mantisbase.utils.hash_password("secret")
        self.assertTrue(mantisbase.utils.verify_password("secret", stored))
        self.assertFalse(mantisbase.utils.verify_password("other", stored))

        safe = mantisbase.utils.sanitize_filename("My Report.txt")
        self.assertNotIn(" ", safe)
        self.assertTrue(safe.endswith(".txt"))
        with self.assertRaises(mantisbase.MantisError) as caught:
            mantisbase.utils.sanitize_filename("payload.exe")
        self.assertEqual(caught.exception.code, 415)

        self.assertTrue(mantisbase.utils.is_allowed_file_extension(".png"))
        self.assertEqual(mantisbase.utils.safe_content_type(".png"), "image/png")
        self.assertTrue(mantisbase.utils.generate_time_based_id())
        self.assertTrue(mantisbase.utils.generate_readable_time_id())
        self.assertGreaterEqual(len(mantisbase.utils.generate_short_id(8)), 8)

    def test_app_metadata(self):
        self.assertEqual(self.app.host, HOST)
        self.assertEqual(self.app.port, STATE["port"])
        self.assertEqual(self.app.db_type, "sqlite3")
        self.assertTrue(self.app.dev_mode)
        self.assertTrue(self.app.skip_admin_setup)
        self.assertGreaterEqual(self.app.uptime_seconds, 0)
        self.assertTrue(self.app.db.connected)
        first = self.app.snowflake_id()
        second = self.app.snowflake_id()
        self.assertTrue(first)
        self.assertNotEqual(first, second)
        self.assertTrue(self.app.router.is_running)
        self.assertEqual(self.app.router.sse.session_count(), 0)

    def test_schema_and_entity_crud(self):
        self.assertIsNone(self.app.validate_schema(NOTES_SCHEMA))
        invalid = self.app.validate_schema({})
        self.assertIsInstance(invalid, str)
        self.assertTrue(invalid)
        self.assertTrue(self.app.has_entity("notes"))
        self.assertTrue(self.app.table_exists("notes"))
        with self.assertRaises(mantisbase.MantisError) as missing:
            self.app.entity("does_not_exist")
        self.assertGreaterEqual(missing.exception.code, 400)
        with self.assertRaises(mantisbase.MantisError):
            self.app.create_schema(NOTES_SCHEMA)

        created = self.app.create_schema(LABELS_SCHEMA)
        self.assertEqual(created["schema"]["name"], "labels")
        listed = self.app.list_schemas()
        self.assertTrue(any(row["schema"]["name"] == "labels" for row in listed))
        fetched = self.app.get_schema(created["id"])
        self.assertEqual(fetched["schema"]["name"], "labels")
        self.app.drop_schema(created["id"])
        self.assertFalse(self.app.has_entity("labels"))

        notes = self.app.entity("notes")
        self.assertEqual(notes.name, "notes")
        self.assertEqual(notes.type, "base")
        self.assertTrue(notes.has_field("title"))
        self.assertIsNotNone(notes.field("title"))
        self.assertEqual(notes.list_rule()["mode"], "public")

        row = notes.create({"title": "Created", "body": "body"})
        row_id = row["id"]
        self.assertEqual(notes.read(row_id)["title"], "Created")
        self.assertTrue(notes.record_exists(row_id))
        notes.update(row_id, {"title": "Updated"})
        self.assertEqual(notes.read(row_id)["title"], "Updated")
        self.assertGreaterEqual(notes.count({"title": "Updated"}), 1)

        page = notes.list_page({"pagination": {"limit": 1}})
        self.assertEqual(len(page["items"]), 1)
        self.assertTrue(page["has_more"])

        found = notes.find_by_columns("Updated", ["title"])
        self.assertEqual(found["id"], row_id)
        notes.remove(row_id)
        self.assertIsNone(notes.read(row_id))
        self.assertFalse(notes.record_exists(row_id))

    def test_database_query_and_transaction(self):
        notes = self.app.entity("notes")
        row = notes.create({"title": "Transactional"})
        row_id = row["id"]
        try:
            selected = self.app.db.query(
                "SELECT id, title FROM notes WHERE id = :id",
                {"id": row_id},
            )
            self.assertEqual(selected, [{"id": row_id, "title": "Transactional"}])

            with self.app.db.transaction() as tx:
                tx.query(
                    "UPDATE notes SET title = :title WHERE id = :id",
                    {"title": "Committed", "id": row_id},
                )
            self.assertEqual(notes.read(row_id)["title"], "Committed")

            with self.assertRaises(RuntimeError):
                with self.app.db.transaction() as tx:
                    tx.query(
                        "UPDATE notes SET title = :title WHERE id = :id",
                        {"title": "Rolled", "id": row_id},
                    )
                    raise RuntimeError("rollback")
            self.assertEqual(notes.read(row_id)["title"], "Committed")
        finally:
            notes.remove(row_id)

    def test_settings_logs_and_files(self):
        original = self.app.settings.get("orgName")
        try:
            self.app.settings.set("orgName", "Py Test")
            self.assertEqual(self.app.settings.get("orgName"), "Py Test")
            self.assertEqual(self.app.settings.configs()["orgName"], "Py Test")
            self.assertIsNone(self.app.settings.get("no_such_setting_key"))
        finally:
            if original is not None:
                self.app.settings.set("orgName", original)

        self.app.logs.info("Python", "binding test", details="ok", data={"n": 1})

        self.assertIn("notes", self.app.files.dir_path("notes"))
        self.assertFalse(self.app.files.file_exists("notes", "missing.txt"))
        self.assertIsNone(self.app.files.get_file_path("notes", "missing.txt"))
        with self.assertRaises(mantisbase.MantisError):
            self.app.files.canonical_path("../outside")

    def test_auth_token_and_api_key(self):
        token = self.app.auth.create_token(
            {"id": "user-1", "entity": "notes"}, timeout=120
        )
        self.assertTrue(token)
        verified = self.app.auth.verify_token(token)
        self.assertTrue(verified["verified"])
        self.assertEqual(verified["claims"]["id"], "user-1")
        self.assertEqual(verified["claims"]["entity"], "notes")
        with self.assertRaises(Exception):
            self.app.auth.create_token({"id": "user-1"})

        created = self.app.auth.api_keys.create_admin(
            "admin-1", "ci", permissions=["read"]
        )
        self.assertTrue(created["key"].startswith("mb_sk_"))
        listed = self.app.auth.api_keys.list_admin()
        self.assertTrue(any(item["id"] == created["id"] for item in listed))
        self.assertTrue(self.app.auth.api_keys.revoke_admin(created["id"]))

    def test_mailer_mock_and_template(self):
        rendered = self.app.mailer.load_template("hello.txt", {"name": "Ada"})
        self.assertEqual(rendered, "Hello Ada")

        self.app.mailer.clear_rate_limits()
        self.app.mailer.clear_sent_messages()
        self.app.mailer.configure(
            {
                "host": "mock",
                "port": 587,
                "from": "app@example.com",
                "tls": False,
            }
        )
        self.assertTrue(self.app.mailer.is_configured)
        self.assertEqual(self.app.mailer.config["host"], "mock")
        self.assertEqual(self.app.mailer.config["from"], "app@example.com")
        self.app.mailer.send(
            "to@example.com", "Hi", html="<p>Hi</p>", text="Hi"
        )
        sent = self.app.mailer.sent_messages()
        self.assertEqual(len(sent), 1)
        self.assertEqual(sent[0]["to"], "to@example.com")
        self.assertEqual(sent[0]["subject"], "Hi")
        self.assertEqual(sent[0]["text"], "Hi")

    def test_http_routes(self):
        status, _, payload = _http("GET", "/api/v1/health")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(payload), {"status": "OK"})

        status, _, payload = _http("GET", "/hello")
        self.assertEqual(status, 200, payload)
        self.assertEqual(json.loads(payload), {"message": "Hello, World!"})

        note_path = "/notes/" + urllib.parse.quote(STATE["note_id"])
        status, _, payload = _http("GET", note_path)
        self.assertEqual(status, 200, payload)
        self.assertEqual(
            json.loads(payload),
            {"id": STATE["note_id"], "title": "Hello from Python"},
        )

        status, _, payload = _http("GET", "/search?q=py", headers={"X-Mark": "yes"})
        self.assertEqual(status, 200, payload)
        self.assertEqual(json.loads(payload), {"q": "py", "mark": "yes"})

        status, _, payload = _http(
            "POST",
            "/echo",
            body=json.dumps({"title": "posted"}).encode(),
            headers={"Content-Type": "application/json"},
        )
        self.assertEqual(status, 201, payload)
        self.assertEqual(json.loads(payload), {"got": {"title": "posted"}})

        status, _, payload = _http("GET", "/blocked")
        self.assertEqual(status, 401, payload)
        self.assertEqual(json.loads(payload)["error"], "blocked")

        status, _, payload = _http("GET", "/admin-only")
        self.assertEqual(status, 401, payload)

        status, _, payload = _http(
            "GET", "/whoami", headers={"Authorization": "Bearer sample-token"}
        )
        self.assertEqual(status, 200, payload)
        self.assertEqual(json.loads(payload)["token"], "sample-token")

        status, _, payload = _http("PATCH", "/patched")
        self.assertEqual(status, 200, payload)
        self.assertEqual(json.loads(payload), {"patched": True})

        status, _, payload = _http("PUT", "/replaced")
        self.assertEqual(status, 200, payload)
        self.assertEqual(json.loads(payload), {"replaced": True})

        status, headers, _ = _http("GET", "/old")
        self.assertEqual(status, 301)
        self.assertIn("/hello", headers["location"])

        status, _, payload = _http("GET", "/api/v1/entities/notes")
        self.assertEqual(status, 200, payload)
        body = json.loads(payload)
        titles = [item["title"] for item in body["data"]["items"]]
        self.assertIn("Hello from Python", titles)

        status, _, _ = _http("GET", "/api/v1/entities/does_not_exist")
        self.assertEqual(status, 404)

        status, _, payload = _http("GET", "/mb")
        self.assertEqual(status, 200)
        self.assertGreater(len(payload), 100)

        self.app.router.broadcast(
            {
                "entity": "notes",
                "row_id": STATE["note_id"],
                "type": "update",
                "timestamp": 1,
                "new_data": {"title": "Hello from Python"},
            }
        )
        self.app.rt.notify_change()


if __name__ == "__main__":
    unittest.main()
