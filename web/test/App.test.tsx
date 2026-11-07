import { render, screen } from "@testing-library/preact";
import { describe, expect, it } from "vitest";

import { App } from "../src/App";

describe("App", () => {
  it("zeigt die Überschrift", () => {
    render(<App />);
    expect(screen.getByRole("heading", { name: "Räucherofen" })).toBeDefined();
  });
});
