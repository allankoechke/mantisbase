app.router().addRoute("GET", "/api/v1/test/scripting/ping", function (req, res) {
    res.json(200, JSON.stringify({ pong: true, source: "test-script" }));
});

app.router().addRoute("GET", "/api/v1/test/scripting/settings-count", function (req, res) {
    var row = app.db().query("SELECT COUNT(*) AS cnt FROM __settings");
    var count = row && row.cnt !== undefined ? row.cnt : 0;
    res.json(200, JSON.stringify({ settings_count: count }));
});

app.router().addRoute("GET", "/api/v1/test/scripting/mw-abort", function (req, res) {
    res.json(200, JSON.stringify({ reached: true }));
}, function (req, res) {
    return false;
});

app.router().addRoute("GET", "/api/v1/test/scripting/protected", function (req, res) {
    res.json(200, JSON.stringify({ protected: true }));
}, middlewares.getAuthToken());

console.log("MantisBase test scripting routes registered");
