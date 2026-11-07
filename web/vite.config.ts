// Build und Tests der Weboberfläche (M9, E-043).
import preact from "@preact/preset-vite";
import { defineConfig } from "vitest/config";

// Beim Entwickeln mit npm run dev gehen API und WebSocket an das echte Gerät,
// wenn der Rechner mit seinem WLAN verbunden ist.
const DEVICE = "192.168.4.1";

export default defineConfig({
  plugins: [preact()],
  build: {
    sourcemap: false,
  },
  server: {
    proxy: {
      "/api": `http://${DEVICE}`,
      "/ws": { target: `ws://${DEVICE}`, ws: true },
    },
  },
  test: {
    environment: "jsdom",
    include: ["test/**/*.test.ts", "test/**/*.test.tsx"],
    setupFiles: ["test/setup.ts"],
  },
});
