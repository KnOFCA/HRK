"""Verify pause/resume, seek, foreground, and XComponent recovery on a connected target."""
import argparse,importlib.util,json,re,time
from pathlib import Path
s=importlib.util.spec_from_file_location('device_test',Path(__file__).with_name('device-test.py'));m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
p=argparse.ArgumentParser();p.add_argument('--hdc',required=True);p.add_argument('--prefix',default='emulator');a=p.parse_args();h=a.hdc;prefix=a.prefix
m.click(h,'AutoPlay');nodes=m.snapshot(h,prefix+'-lifecycle-controls')
def click(text):
 n=next(n for n in nodes if n.get('type')=='Button' and n.get('text')==text)
 x,y,r,b=map(int,re.findall(r'\d+',n['bounds']));m.run(h,'shell','uitest','uiInput','click',str((x+r)//2),str((y+b)//2))
def song(name):
 n=m.snapshot(h,name)
 return float(next(v['text'][:-2] for v in n if re.fullmatch(r'\d+\.\d{3} s',v.get('text',''))))
click('开始');time.sleep(4.5);click('暂停');before=song(prefix+'-paused-before');time.sleep(2);after=song(prefix+'-paused-after');assert before==after and 4.5<=before<=6.5,(before,after)
resumeHost=time.monotonic();click('恢复');resumed=song(prefix+'-resumed');resumeElapsed=time.monotonic()-resumeHost;assert .2<resumed-after<2.5,(resumed,after,resumeElapsed)  # RPC latency is not audio elapsed time.
click('Seek 50%');click('暂停');seek=song(prefix+'-seek');assert 4<=seek<5.5,seek
click('恢复');m.run(h,'shell','uitest','uiInput','keyEvent','Home');time.sleep(.5);m.run(h,'shell','aa','start','-b',m.BUNDLE,'-a','EntryAbility')
foreground=song(prefix+'-foreground');time.sleep(1);frozen=song(prefix+'-foreground-frozen');assert foreground==frozen
m.click(h,'重建画面');time.sleep(.5);rebuilt=song(prefix+'-rebuilt');assert rebuilt==frozen
m.click(h,'调整尺寸');m.export(h,prefix+'-lifecycle')
metrics=json.loads((m.OUT/(prefix+'-lifecycle-metrics.json')).read_text());assert metrics['surfaceCreates']>=2 and metrics['surfaceDestroys']>=1 and metrics['surfaceResizes']>=1
result=dict(pauseBefore=before,pauseAfter=after,resumed=resumed,resumeElapsed=resumeElapsed,seek=seek,foreground=foreground,foregroundFrozen=frozen,rebuilt=rebuilt)
(m.OUT/(prefix+'-lifecycle-assertions.json')).write_text(json.dumps(result,indent=2));print(result)
m.click(h,'调整尺寸');m.click(h,'Play')
