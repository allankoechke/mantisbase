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

// db.query leases its own connection. Statements that must commit or roll back
// together go through db.queryOn on one session from db.session().
app.router().get("/api/v1/test/scripting/db", function (req, res) {
    const db = app.db();
    const sess = db.session();
    const connected = db.connected === true && sess.connected === true;
    const backend = sess.getBackendName();

    db.queryOn(sess, "CREATE TABLE IF NOT EXISTS mb_script_probe (id TEXT PRIMARY KEY, note TEXT)");
    db.queryOn(sess, "DELETE FROM mb_script_probe");

    sess.begin();
    db.queryOn(sess, "INSERT INTO mb_script_probe (id, note) VALUES (:id, :note)", {
        id: "keep",
        note: "committed"
    });
    const lastQuery = sess.getLastQuery();
    sess.commit();

    sess.begin();
    db.queryOn(sess, "INSERT INTO mb_script_probe (id, note) VALUES (:id, :note)", {
        id: "drop",
        note: "rolled-back"
    });
    sess.rollback();

    const kept = db.query("SELECT id, note FROM mb_script_probe WHERE id = :id", { id: "keep" });
    const dropped = db.query("SELECT id FROM mb_script_probe WHERE id = :id", { id: "drop" });
    db.queryOn(sess, "DROP TABLE IF EXISTS mb_script_probe");

    res.json(200, {
        connected: connected,
        backend: backend,
        committed: kept !== null && kept.note === "committed",
        rolledBack: dropped === null,
        lastQueryHasInsert: lastQuery.indexOf("INSERT") >= 0
    });
});

app.router().get("/api/v1/test/scripting/helpers", function (req, res) {
    const timeId = generateTimeBasedId();
    const readable = generateReadableTimeId();
    const shortId = generateShortId(8);
    const hashed = hashPassword("script-secret");
    const filename = sanitizeFilename("note.txt");

    app.logs().debug("scripting test");
    app.logs().trace("scripting test");
    app.logs().info("scripting test");
    app.logs().warn("scripting test");
    app.logs().error("scripting test");
    app.logs().critical("scripting test");

    res.json(200, {
        timeId: timeId.length > 0,
        readable: readable.length > 0,
        shortId: shortId.length === 8,
        env: getEnvOrDefault("MB_DISABLE_RATE_LIMIT", "0") === "1",
        password: verifyPassword("script-secret", hashed) === true,
        extension: isAllowedFileExtension(".txt") === true && isAllowedFileExtension(".exe") === false,
        contentType: safeContentType(".txt").length > 0 && safeContentType(".unknown") === "application/octet-stream",
        flag: strToBool("yes") === true && strToBool("no") === false,
        filename: filename.indexOf("note") >= 0,
        logged: true
    });
});

app.router().get("/api/v1/test/scripting/mailer", function (req, res) {
    const mailer = app.mailer();
    mailer.clearSentMessages();
    mailer.clearRateLimits();
    mailer.configure({
        host: "mock",
        port: 587,
        user: "mailer",
        password: "s3cret",
        from: "app@example.com",
        tls: true
    });

    const masked = mailer.config();
    mailer.configure({
        host: masked.host,
        port: masked.port,
        user: masked.user,
        password: masked.password,
        from: masked.from,
        tls: masked.tls
    });

    const sent = mailer.send("user@example.com", "Hello", "<p>Hello</p>", "Hello");
    const messages = mailer.sentMessages();
    const template = mailer.loadTemplate("login_otp.txt", {
        app_name: "Probe",
        otp: "123456",
        expires_minutes: "10"
    });
    const allowed = mailer.checkRateLimit("user@example.com", 5);
    const stillConfigured = mailer.isConfigured() === true;
    mailer.applySettings({
        smtp: {
            host: "mock",
            port: 2525,
            user: "mailer",
            password: "s3cret",
            from: "app@example.com",
            tls: false
        }
    });
    const applied = mailer.config();

    mailer.clearSentMessages();
    mailer.clearRateLimits();
    mailer.applySettings({});

    res.json(200, {
        configured: masked.host === "mock" && masked.password === "********" && masked.tls === true,
        maskKept: stillConfigured,
        sent: sent === true && messages.length === 1 && messages[0].subject === "Hello" && messages[0].text === "Hello",
        template: template.indexOf("123456") >= 0,
        allowed: allowed === true,
        appliedPort: applied.port === 2525 && applied.tls === false
    });
});

app.router().get("/api/v1/test/scripting/auth", function (req, res) {
    const auth = app.auth();
    const mailer = app.mailer();
    mailer.clearSentMessages();
    mailer.clearRateLimits();
    mailer.configure({
        host: "mock",
        port: 587,
        user: "mailer",
        password: "s3cret",
        from: "app@example.com",
        tls: false
    });

    const token = auth.createToken(JSON.stringify({ id: "script-user", entity: "mb_admins" }), 120);
    const verified = JSON.parse(auth.verifyToken(token));
    const timeout = auth.sessionTimeoutSeconds("mb_admins", 90);
    auth.rejectUnverifiedLogin("mb_admins", { id: "script-user", email_verified: false });
    const otp = auth.requestLoginOtp("mb_admins", "nobody@example.com");
    auth.notifyEmailVerification("mb_admins", "missing-user");
    const email = auth.requestEmailVerification("mb_admins", "nobody@example.com", "localhost");
    const reset = auth.requestPasswordReset("mb_admins", "nobody@example.com", "localhost");
    const quiet = mailer.sentMessages().length === 0;

    const keys = auth.apiKey();
    const created = keys.create("mb_admins", "script-user", "script token", ["read"], "");
    const listed = keys.list("mb_admins", "script-user");
    const revoked = keys.revoke(created.id, "mb_admins", "script-user");
    const admin = keys.createAdmin("script-admin", "admin token", null, "");
    const admins = keys.listAdmin();
    const adminRevoked = keys.revokeAdmin(admin.id);

    const oauth = auth.oauth();
    const providers = oauth.listProviders();
    var hasGoogle = false;
    for (var i = 0; i < providers.length; i++) {
        if (providers[i].name === "google") hasGoogle = true;
    }
    const enabled = oauth.providers("mb_admins");
    const linked = oauth.linkedAccounts("mb_admins", "nobody");
    const unlinked = oauth.unlinkAccount("mb_admins", "nobody", "google");

    mailer.clearSentMessages();
    mailer.clearRateLimits();
    mailer.applySettings({});

    var listedCreated = false;
    for (var i = 0; i < listed.length; i++) {
        if (listed[i].id === created.id) listedCreated = true;
    }
    var listedAdmin = false;
    for (var i = 0; i < admins.length; i++) {
        if (admins[i].id === admin.id) listedAdmin = true;
    }

    res.json(200, {
        tokenVerified: verified.verified === true && verified.claims.id === "script-user",
        timeout: timeout >= 90,
        otpSent: otp.sent === true,
        emailSent: email.sent === true,
        resetSent: reset.sent === true,
        quiet: quiet,
        keyPrefix: created.key.indexOf("mb_sk_") === 0 && created.permissions[0] === "read",
        keyListed: listedCreated && revoked === true,
        adminKey: admin.permissions.length === 0 && listedAdmin && adminRevoked === true,
        google: hasGoogle,
        providersArray: enabled.length >= 0 && linked.length === 0 && unlinked === false
    });
});

app.router().get("/api/v1/test/scripting/files", function (req, res) {
    const files = app.files();
    files.createDir("script_probe");
    const dir = files.dirPath("script_probe", false);
    const path = files.filePath("script_probe", "note.txt");
    const exists = files.fileExists("script_probe", "note.txt");
    const missing = files.getFilePath("script_probe", "note.txt");
    files.removeFiles("script_probe", ["note.txt"]);
    const removed = files.removeFile("script_probe", "note.txt");
    const base = files.baseDir();
    const canonical = files.canonicalPath(base);
    const inside = files.isCanonical(base);
    files.renameDir("script_probe", "script_probe_renamed");
    files.deleteDir("script_probe_renamed");

    res.json(200, {
        dir: dir.length > 0 && path.length > 0,
        exists: exists === false,
        missing: missing === "",
        removed: removed === false,
        canonical: canonical.length > 0 && inside === true
    });
});

app.router().get("/api/v1/test/scripting/realtime", function (req, res) {
    app.rt().addDbHooks("mb_admins");
    app.rt().notifyChange();
    app.router().broadcastChange(JSON.stringify({
        type: "update",
        entity: "mb_admins",
        row_id: "script",
        timestamp: "",
        new_data: { id: "script" }
    }));
    app.rt().dropDbHooks("mb_admins");
    res.json(200, { hooked: true });
});

// MB_DISABLE_ADMIN_ON_FIRST_BOOT is set by the test harness, so this gate blocks.
app.router().get("/api/v1/test/scripting/env-blocked", function (req, res) {
    res.json(200, { reached: true });
}, middlewares.envGate("MB_DISABLE_ADMIN_ON_FIRST_BOOT", true));

app.router().get("/api/v1/test/scripting/env-open", function (req, res) {
    res.json(200, { reached: true });
}, middlewares.envGate("MB_THIS_VAR_IS_UNSET", true));

// The harness sets MB_DISABLE_RATE_LIMIT=1, so the limiter lets the request through.
app.router().get("/api/v1/test/scripting/rate-limit", function (req, res) {
    res.json(200, { reached: true });
}, middlewares.rateLimit(1, 60, false));
