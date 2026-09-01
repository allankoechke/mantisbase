// Custom routes loaded at server startup (requires MB_SCRIPTING_ENABLED).
// Deprecated: use scripts/main.mb.js instead.

app.router().addRoute("GET", "/api/v1/custom/health", function (req, res) {
    res.json(200, JSON.stringify({ status: "ok", source: "script" }));
});

app.router().addRoute("GET", "/api/v1/custom/settings-count", function (req, res) {
    var db = app.db();
    var row = db.query("SELECT COUNT(*) AS cnt FROM __settings");
    var count = row && row.cnt !== undefined ? row.cnt : 0;
    res.json(200, JSON.stringify({ settings_count: count }));
});

console.log("MantisBase scripting: custom routes registered (index.mantis.js — prefer main.mb.js)");
