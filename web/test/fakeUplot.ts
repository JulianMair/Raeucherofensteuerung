// Ersatz für uPlot in den Tests: jsdom hat kein Canvas. Der Fake merkt sich,
// womit er angelegt und gefüttert wurde.
import type uPlot from "uplot";

export class FakePlot {
  static instances: FakePlot[] = [];
  data: uPlot.AlignedData;
  destroyed = false;

  constructor(
    readonly options: uPlot.Options,
    data: uPlot.AlignedData,
    readonly target: HTMLElement,
  ) {
    this.data = data;
    FakePlot.instances.push(this);
  }

  setData(data: uPlot.AlignedData): void {
    this.data = data;
  }

  setSize(): void {}

  destroy(): void {
    this.destroyed = true;
  }
}
