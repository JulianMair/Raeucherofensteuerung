import { render } from "@testing-library/preact";
import { describe, expect, it } from "vitest";

import { LiveChart } from "../src/components/LiveChart";
import { appendState, emptyHistory } from "../src/history";
import { FakePlot } from "./fakeUplot";
import { stateWith } from "./fixtures";

function onlyPlot(): FakePlot {
  expect(FakePlot.instances).toHaveLength(1);
  const plot = FakePlot.instances[0];
  if (plot === undefined) {
    throw new Error("keine Kurve");
  }
  return plot;
}

describe("Livekurve", () => {
  /* Vom Nutzer gewählt: Temperaturen und Sollwert, kein Tastgrad */
  it("zeichnet Garraum, beide Kerne und den Sollwert über der Zeit", () => {
    render(<LiveChart history={emptyHistory()} version={0} />);

    const plot = onlyPlot();
    expect(plot.options.series.map((s) => s.label)).toEqual(["Zeit", "Garraum", "Kern 1", "Kern 2", "Sollwert"]);
    expect(plot.options.scales?.x?.time).toBe(true);
  });

  it("übernimmt neue Punkte, ohne die Kurve neu anzulegen", () => {
    const history = emptyHistory();
    const view = render(<LiveChart history={history} version={0} />);

    appendState(history, stateWith({ time_ms: 1000 }), 2000);
    view.rerender(<LiveChart history={history} version={1} />);

    const plot = onlyPlot();
    expect(plot.data[0]).toEqual([2]);
    expect(plot.data[1]).toEqual([79.94]);
  });

  it("räumt die Kurve beim Verlassen auf", () => {
    const view = render(<LiveChart history={emptyHistory()} version={0} />);
    view.unmount();
    expect(onlyPlot().destroyed).toBe(true);
  });
});
