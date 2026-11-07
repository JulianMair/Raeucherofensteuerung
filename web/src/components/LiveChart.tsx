import "uplot/dist/uPlot.min.css";

import { useEffect, useRef } from "preact/hooks";
import uPlot from "uplot";

import type { History } from "../history";

interface Props {
  history: History;
  version: number; /* zählt bei jedem neuen Punkt hoch, damit die Kurve neu zeichnet */
}

const HEIGHT_PX = 260;

/* Linien in der Reihenfolge der Spalten von History */
const SERIES: uPlot.Series[] = [
  {},
  { label: "Garraum", stroke: "#c2410c", width: 2 },
  { label: "Kern 1", stroke: "#1d4ed8", width: 2 },
  { label: "Kern 2", stroke: "#15803d", width: 2 },
  { label: "Sollwert", stroke: "#6b7280", width: 1.5, dash: [6, 4] },
];

function columns(history: History): uPlot.AlignedData {
  return [history.time_s, history.chamber, history.probe_1, history.probe_2, history.setpoint];
}

/* Livekurve mit uPlot (WEB-01, WEB-02): Garraum, Kerne und Sollwert in °C */
export function LiveChart({ history, version }: Props) {
  const container = useRef<HTMLDivElement>(null);
  const plot = useRef<uPlot | null>(null);

  useEffect(() => {
    const element = container.current;
    if (element === null) {
      return;
    }
    const options: uPlot.Options = {
      width: element.clientWidth,
      height: HEIGHT_PX,
      series: SERIES,
      axes: [{}, { label: "°C" }],
      scales: { x: { time: true } },
    };
    const chart = new uPlot(options, columns(history), element);
    plot.current = chart;
    /* Bei Drehen oder Größenänderung des Fensters mitwachsen */
    const resize = () => chart.setSize({ width: element.clientWidth, height: HEIGHT_PX });
    window.addEventListener("resize", resize);
    return () => {
      window.removeEventListener("resize", resize);
      chart.destroy();
      plot.current = null;
    };
    // Nur einmal anlegen; neue Daten kommen über setData unten
  }, []);

  useEffect(() => {
    plot.current?.setData(columns(history));
  }, [history, version]);

  return (
    <section class="panel" aria-label="Livekurve">
      <h2>Verlauf seit dem Öffnen der Seite</h2>
      <div ref={container} class="chart" />
    </section>
  );
}
