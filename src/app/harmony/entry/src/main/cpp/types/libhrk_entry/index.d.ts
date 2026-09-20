export const initialize: (chart: string, audio: ArrayBuffer, mode: number, replay: ArrayBuffer) => string;
export const command: (name: string, value?: number) => string;
export const status: () => string;
export const exportReplay: () => ArrayBuffer;
export const exportResult: () => string;
export const exportMetrics: () => string;
