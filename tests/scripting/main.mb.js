app.router().addRoute("GET", "/api/v1/test/scripting/ping", function (req, res) {
    res.json(200, { pong: true, source: "test-script" });
});

app.router().addRoute("GET", "/api/v1/test/scripting/settings-count", function (req, res) {
    var row = app.db().query("SELECT COUNT(*) AS cnt FROM mb_store");
    var count = row && row.cnt !== undefined ? row.cnt : 0;
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

console.log("MantisBase test scripting routes registered");
