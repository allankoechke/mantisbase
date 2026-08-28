// Custom routes loaded at server startup (requires MB_SCRIPTING_ENABLED).
// Place as scripts/index.mantis.js (or set --scriptsDir).

app.router().addRoute("GET", "/api/v1/custom/health", function (req, res) {
    res.json(200, JSON.stringify({ status: "ok", source: "script" }));
});

app.router().addRoute("GET", "/api/v1/custom/settings-count", function (req, res) {
    var db = app.db();
    var rows = db.query("SELECT COUNT(*) AS cnt FROM __settings");
    var count = rows && rows.length ? rows[0].cnt : 0;
    res.json(200, JSON.stringify({ settings_count: count }));
});

console.log("MantisBase scripting: custom routes registered");
