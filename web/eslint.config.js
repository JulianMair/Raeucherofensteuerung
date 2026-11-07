// Lint der Weboberfläche: empfohlene Regeln von ESLint und typescript-eslint,
// für TypeScript mit Typprüfung (E-043).
import js from "@eslint/js";
import { defineConfig } from "eslint/config";
import globals from "globals";
import tseslint from "typescript-eslint";

export default defineConfig(
  { ignores: ["dist/", "src/api/schema.d.ts"] },
  js.configs.recommended,
  tseslint.configs.recommendedTypeChecked,
  {
    languageOptions: {
      globals: globals.browser,
      parserOptions: { projectService: true, tsconfigRootDir: import.meta.dirname },
    },
  },
  // Diese Datei selbst ist JavaScript und gehört zu keinem TypeScript-Projekt
  { files: ["**/*.js"], extends: [tseslint.configs.disableTypeChecked], languageOptions: { globals: globals.node } },
);
