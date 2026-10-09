app.router().addRoute("GET", "/api/v1/test/scripting/ping", function (req, res) {
    res.json(200, { pong: true, source: "test-script" });
});

app.router().addRoute("GET", "/api/v1/test/scripting/settings-count", function (req, res) {
    const row = app.db().query("SELECT COUNT(*) AS cnt FROM mb_store");
    const count = row && row.cnt !== undefined ? row.cnt : 0;
    res.json(200, { settings_count: count | 0 });
});

app.router().addRoute("GET", "/api/v1/test/scripting/mw-abort", function (req, res) {
    res.json(200, { reached: true });
}, function (req, res) {
    res.json(403, { error: "Access denied", data: undefined, status: 403 });
    return false;
});

app.router().addRoute("GET", "/api/v1/test/scripting/protected", function (req, res) {
    res.json(200, { protected: true });
}, middlewares.requireEntityAuth("test_users"));

// Per-method shorthand bindings (equivalent to addRoute with a fixed method).
app.router().get("/api/v1/test/scripting/shorthand-get", function (req, res) {
    res.json(200, { ok: true, method: "get" });
});

app.router().post("/api/v1/test/scripting/shorthand-post", function (req, res) {
    res.json(200, { ok: true, method: "post" });
});

app.router().put("/api/v1/test/scripting/shorthand-put", function (req, res) {
    res.json(200, { ok: true, method: "put" });
});

app.router().patch("/api/v1/test/scripting/shorthand-patch", function (req, res) {
    res.json(200, { ok: true, method: "patch" });
});

app.router()["delete"]("/api/v1/test/scripting/shorthand-delete", function (req, res) {
    res.json(200, { ok: true, method: "delete" });
});

// Path redirects (default 302 + permanent 301).
app.router().redirect("/api/v1/test/scripting/old-path", "/api/v1/test/scripting/shorthand-get");
app.router().redirect("/api/v1/test/scripting/old-permanent", "/api/v1/test/scripting/shorthand-get", 301);

app.logs().info("Hello World!")
console.log("MantisBase test scripting routes registered");
