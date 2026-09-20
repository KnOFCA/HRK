import { UIAbility } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import hrk from 'libhrk_entry.so';
export default class EntryAbility extends UIAbility {
  onWindowStageCreate(stage: window.WindowStage): void { stage.loadContent('pages/Index', (error) => { if (error.code) { console.error('HRK page load failed: ' + error.code); } }); }
  onBackground(): void { hrk.command('background'); }
  onDestroy(): void { hrk.command('stop'); }
}
