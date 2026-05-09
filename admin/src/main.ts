const app = document.getElementById("app");
if (app) {
  app.innerHTML = `
    <h1>MantisBase Admin (Vite)</h1>
    <p>Development server proxies <code>/api</code> to the Rust backend.</p>
    <p>OpenAPI: <a href="/api/v1/openapi.json" target="_blank">/api/v1/openapi.json</a></p>
  `;
}
