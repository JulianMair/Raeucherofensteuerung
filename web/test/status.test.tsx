import { render, screen } from "@testing-library/preact";
import { describe, expect, it } from "vitest";

import type { Fault, State } from "../src/api/types";
import { StatusPanel } from "../src/components/StatusPanel";
import { FAULT_LABELS, STATE_LABELS } from "../src/labels";
import { stateWith } from "./fixtures";

const ALL_STATES = Object.keys(STATE_LABELS) as State[];
const ALL_FAULTS = (Object.keys(FAULT_LABELS) as Fault[]).filter((fault) => fault !== "none");

describe("Statusanzeige", () => {
  /* WEB-06: Anzeige aller Zustände */
  it.each(ALL_STATES)("zeigt den Zustand %s mit Text und eigener Klasse", (state) => {
    render(<StatusPanel state={stateWith({ state })} stale={false} />);

    const shown = screen.getByTestId("state");
    expect(shown.textContent).toBe(STATE_LABELS[state]);
    expect(shown.className).toContain(`state-${state}`);
  });

  it.each(["idle", "heating", "holding", "cooling"] as State[])("zeigt in %s kein Band", (state) => {
    render(<StatusPanel state={stateWith({ state })} stale={false} />);
    expect(screen.queryByRole("alert")).toBeNull();
  });

  /* WEB-03, WEB-06: Sicherer Zustand mit der verriegelten Ursache */
  it.each(ALL_FAULTS)("zeigt im sicheren Zustand die Ursache %s", (fault) => {
    render(<StatusPanel state={stateWith({ state: "safe_state", latched_fault: fault })} stale={false} />);

    const band = screen.getByRole("alert");
    expect(band.textContent).toContain("Sicherer Zustand");
    expect(band.textContent).toContain(FAULT_LABELS[fault]);
  });

  /* FEHLER zeigt die aktive Ursache, nicht die verriegelte (E-020) */
  it("zeigt in FEHLER die aktive Ursache", () => {
    render(
      <StatusPanel
        state={stateWith({ state: "fault", active_fault: "chamber_unassigned", latched_fault: "none" })}
        stale={false}
      />,
    );
    expect(screen.getByRole("alert").textContent).toContain(FAULT_LABELS.chamber_unassigned);
  });

  it("zeigt Sollwert, Heizung und die Rollen", () => {
    render(<StatusPanel state={stateWith({})} stale={false} />);

    expect(screen.getByText("80,0 °C")).toBeDefined();
    expect(screen.getByText("46 %")).toBeDefined();
    expect(screen.getByTestId("reading-chamber").textContent).toBe("79,9 °C");
    expect(screen.getByTestId("reading-probe_2").textContent).toBe("nicht zugeordnet");
    expect(screen.getByTestId("reading-smoke_gen").textContent).toBe("fehlt");
  });

  it("zeigt ohne Sollwert einen Strich", () => {
    render(<StatusPanel state={stateWith({ setpoint_c: null })} stale={false} />);
    expect(screen.getByText("–")).toBeDefined();
  });

  it("markiert alte Werte", () => {
    render(<StatusPanel state={stateWith({})} stale={true} />);
    expect(screen.getByLabelText("Status").className).toContain("stale");
  });
});
