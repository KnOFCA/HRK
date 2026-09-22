export const initialize: (chart: string, audio: ArrayBuffer, mode: number, replay: ArrayBuffer) => string;
export const command: (name: string, value?: number) => string;
export const status: () => string;
export const exportReplay: () => ArrayBuffer;
export const exportResult: () => string;
export const exportMetrics: () => string;

export interface InputCaptureMetadata {
  runId: string;
  sourceRevision: string;
  specRevision: string;
  policyVersion: string;
  buildMode: string;
  deviceModel: string;
  osVersion: string;
  chartSha256: string;
  audioSha256: string;
  origin: string;
  actualRefreshHz: number | null;
}
export const beginCapture: (metadata: InputCaptureMetadata) => string;
export const endCapture: () => string;
export const exportCapture: (path: string) => string;
