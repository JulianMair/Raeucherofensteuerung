import { describe, expect, it } from "vitest";

import { formatDuration } from "../src/labels";

describe("Restzeit als Text", () => {
  it.each([
    [0, "unter 1 min"],
    [29, "unter 1 min"],
    [30, "1 min"],
    [3480, "58 min"],
    [3570, "1 h 00 min"],
    [3900, "1 h 05 min"],
    [86400, "24 h 00 min"],
  ])("%i s sind %s", (seconds, text) => {
    expect(formatDuration(seconds)).toBe(text);
  });
});
