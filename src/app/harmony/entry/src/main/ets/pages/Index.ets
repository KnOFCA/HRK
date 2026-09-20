import hrk from 'libhrk_entry.so';
import { common } from '@kit.AbilityKit';
import { fileIo } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';

interface RuntimeStatus {
  time: number; state: number; score: number; perfect: number; miss: number;
  fps: number; frames: number; inputs: number; rejected: number; overflow: number; error: string;
}

@Entry
@Component
struct Index {
  @State info: RuntimeStatus = { time: 0, state: 0, score: 0, perfect: 0, miss: 0,
    fps: 0, frames: 0, inputs: 0, rejected: 0, overflow: 0, error: 'Ok' };
  @State message: string = '正在加载测试资源';
  @State ready: boolean = false;
  @State surfaceVisible: boolean = true;
  @State compactSurface: boolean = false;
  private timer: number = -1;
  private baselineTimer: number = -1;
  private saved: ArrayBuffer = new ArrayBuffer(0);
  private currentPerformance: boolean = false;
  private savedPerformance: boolean = false;

  aboutToAppear(): void {
    this.load(0);
    this.timer = setInterval(() => { this.info = JSON.parse(hrk.status()) as RuntimeStatus; }, 100);
  }
  aboutToDisappear(): void { clearTimeout(this.baselineTimer); clearInterval(this.timer); hrk.command('stop'); }

  async load(mode: number, performance: boolean = false): Promise<void> {
    clearTimeout(this.baselineTimer);
    if (mode === 1 && this.saved.byteLength > 0) { performance = this.savedPerformance; }
    this.currentPerformance = performance;
    this.ready = false;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const chart = await context.resourceManager.getRawFileContent(performance ? 'performance.json' : 'simple_sequence.json');
      const audio = await context.resourceManager.getRawFileContent(performance ? 'performance.wav' : 'short_test.wav');
      let replay = this.saved;
      if (mode === 1 && replay.byteLength === 0) {
        replay = (await context.resourceManager.getRawFileContent('mixed.replay')).buffer as ArrayBuffer;
      }
      this.message = hrk.initialize(util.TextDecoder.create('utf-8').decodeToString(chart),
        audio.buffer as ArrayBuffer, mode, replay);
      this.ready = this.message === 'Ok';
      if (performance && this.ready) {
        this.ready = false;
        this.message = '资源已准备，3 秒后开始基线';
        this.baselineTimer = setTimeout(() => {
          this.ready = true; this.act('start'); this.act('measure');
        }, 3000);
      }
    } catch (error) { this.message = '资源加载失败：' + String(error); }
  }
  act(name: string, value: number = 0): void { if (name === 'stop') { clearTimeout(this.baselineTimer); } this.message = hrk.command(name, value); }
  save(): void {
    try {
      this.saved = hrk.exportReplay();
      this.savedPerformance = this.currentPerformance;
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const path = context.filesDir + '/session.replay';
      const file = fileIo.openSync(path, fileIo.OpenMode.CREATE | fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.TRUNC);
      try { fileIo.writeSync(file.fd, this.saved); } finally { fileIo.closeSync(file); }
      const result = fileIo.openSync(context.filesDir + '/session-result.json', fileIo.OpenMode.CREATE | fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.TRUNC);
      try { fileIo.writeSync(result.fd, hrk.exportResult()); } finally { fileIo.closeSync(result); }
      const metrics = fileIo.openSync(context.filesDir + '/frame-metrics.json', fileIo.OpenMode.CREATE | fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.TRUNC);
      try { fileIo.writeSync(metrics.fd, hrk.exportMetrics()); } finally { fileIo.closeSync(metrics); }
      this.message = 'Replay 已保存：' + path;
    } catch (error) { this.message = '保存失败：' + String(error); }
  }
  build() {
    Column({ space: 10 }) {
      Text('Harmony Rhythm Kernel').fontSize(24).fontColor('#e8f7f3')
      Text('v0.1.0 · ReferenceGame').fontSize(13).fontColor('#80aaa0')
      Row({ space: 20 }) {
        Text(this.info.time.toFixed(3) + ' s').fontColor('#d7f5e9').fontSize(14)
        Text('PERFECT ' + this.info.perfect).fontColor('#d7f5e9').fontSize(14)
        Text('MISS ' + this.info.miss).fontColor('#d7f5e9').fontSize(14)
        Text('Score ' + this.info.score).fontColor('#d7f5e9').fontSize(14)
      }
      Column() {
        if (this.surfaceVisible) {
          XComponent({ id: 'hrk-surface', type: XComponentType.SURFACE, libraryname: 'hrk_entry' })
            .width(this.compactSurface ? '80%' : '100%').height('100%')
        }
      }.width('100%').layoutWeight(1)
      Text('FPS ' + this.info.fps.toFixed(0) + ' · 输入 ' + this.info.inputs +
        ' · 迟到 ' + this.info.rejected + ' · 溢出 ' + this.info.overflow).fontColor('#80aaa0').fontSize(12)
      Row({ space: 8 }) {
        Button('Play').onClick(() => { this.load(0); })
        Button('Replay').onClick(() => { this.load(1); })
        Button('AutoPlay').onClick(() => { this.load(2); })
        Button('开始').enabled(this.ready).onClick(() => { this.act('start'); })
      }
      Row({ space: 8 }) {
        Button('暂停').onClick(() => { this.act('pause'); })
        Button('恢复').onClick(() => { this.act('resume'); })
        Button('Seek 50%').onClick(() => { this.act('seek', 4); })
      }
      Row({ space: 8 }) {
        Button('停止').onClick(() => { this.act('stop'); })
        Button('保存 Replay').onClick(() => { this.save(); })
        Button('120 Hz').onClick(() => { this.act('120hz'); })
        Button('60s 基线').onClick(() => { this.load(2, true); })
      }
      Row({ space: 8 }) {
        Button('重建画面').onClick(() => {
          this.surfaceVisible = false;
          setTimeout(() => { this.surfaceVisible = true; }, 300);
        })
        Button('调整尺寸').onClick(() => { this.compactSurface = !this.compactSurface; })
      }
      Text(this.message === 'Ok' ? (this.info.state === 3 ? '播放中' : this.info.state === 4 ? '已暂停' : this.info.state === 5 ? '已结束' : '就绪') + ' · 音符到达横线时点击对应位置' : this.message)
        .fontSize(12).fontColor('#a9c7c0').maxLines(3)
      Text('Native: ' + this.info.error).fontSize(11).fontColor('#80aaa0')
    }.padding(16).width('100%').height('100%').backgroundColor('#0a1525')
  }
}
