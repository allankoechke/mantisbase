import { defineConfig } from "vite";

export default defineConfig(({ command }) => ({
  // Production bundle is served under `/mb` by the Rust binary; dev keeps `/` for simpler Vite URLs.
  base: command === "build" ? "/mb/" : "/",
  root: ".",
  build: {
    outDir: "../public/mb-dist",
    emptyOutDir: true,
  },
  server: {
    proxy: {
      "/api": "http://127.0.0.1:7070",
    },
  },
}));
