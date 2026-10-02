// Custom routes loaded at server startup (requires MB_SCRIPTING_ENABLED).
// Place as scripts/main.mb.js (or set --scriptsDir).

app.router().addRoute("GET", "/api/v1/custom/health", function (req, res) {
    res.json(200, JSON.stringify({ status: "ok", source: "script" }));
});

app.router().addRoute("GET", "/api/v1/custom/settings-count", function (req, res) {
    var db = app.db();
    var row = db.query("SELECT COUNT(*) AS cnt FROM __settings");
    var count = row && row.cnt !== undefined ? row.cnt : 0;
    res.json(200, JSON.stringify({ settings_count: count }));
});

// Per-method shorthands (equivalent to addRoute with a fixed method)
app.router().get("/api/v1/custom/ping", function (req, res) {
    res.json(200, JSON.stringify({ status: "ok", source: "script", method: "get" }));
});

app.router().post("/api/v1/custom/echo", function (req, res) {
    res.json(200, JSON.stringify({ status: "ok", source: "script", method: "post" }));
});

// Redirect an old path to the health endpoint (302 Found, GET-only by default).
// Optional 3xx status + method constraints: redirect(from, to, status, ["GET", "POST"])
app.router().redirect("/api/v1/custom/health-old", "/api/v1/custom/health");

console.log("MantisBase scripting: custom routes registered");
