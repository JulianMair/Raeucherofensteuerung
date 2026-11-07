import "uplot/dist/uPlot.min.css";

import { useEffect, useRef } from "preact/hooks";
import uPlot from "uplot";

import type { History } from "../history";
import { formatTemperature } from "../labels";

interface Props {
  history: History;
  version: number; /* zählt bei jedem neuen Punkt hoch, damit die Kurve neu zeichnet */
}

const HEIGHT_PX = 260;
/* Unter 10 Minuten Spanne zeigt die Zeitachse auch Sekunden */
const SECONDS_BELOW_S = 600;

/* Uhrzeit auf Deutsch, uPlot zeigt sonst amerikanisches Format */
function clock(seconds: number, withSeconds: boolean): string {
  return new Date(seconds * 1000).toLocaleTimeString("de-DE", {
    hour: "2-digit",
    minute: "2-digit",
    second: withSeconds ? "2-digit" : undefined,
  });
}

function temperatureValue(_plot: uPlot, value: number | null): string {
  return formatTemperature(value);
}

/* Linien in der Reihenfolge der Spalten von History */
const SERIES: uPlot.Series[] = [
  { label: "Zeit", value: (_plot, value: number | null) => (value === null ? "–" : clock(value, true)) },
  { label: "Garraum", stroke: "#c2410c", width: 2, value: temperatureValue },
  { label: "Kern 1", stroke: "#1d4ed8", width: 2, value: temperatureValue },
  { label: "Kern 2", stroke: "#15803d", width: 2, value: temperatureValue },
  { label: "Sollwert", stroke: "#6b7280", width: 1.5, dash: [6, 4], value: temperatureValue },
];

const AXES: uPlot.Axis[] = [
  {
    values: (plot, splits) => {
      const span = (plot.scales.x?.max ?? 0) - (plot.scales.x?.min ?? 0);
      return splits.map((split) => clock(split, span < SECONDS_BELOW_S));
    },
  },
  { label: "°C" },
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
      axes: AXES,
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
