"""Exercise the native touch path with platform-injected events, and verify its replay."""
import argparse,importlib.util,json,re,struct,subprocess,time
from pathlib import Path
spec=importlib.util.spec_from_file_location('device_test',Path(__file__).with_name('device-test.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p=argparse.ArgumentParser();p.add_argument('--hdc',required=True);p.add_argument('--prefix',default='emulator');args=p.parse_args();h=args.hdc;prefix=args.prefix
m.click(h,'Play');time.sleep(.5)
nodes=m.snapshot(h,prefix+'-input-before')
def bounds(kind,text=None):
    matches=[n for n in nodes if n.get('type')==kind and (text is None or n.get('text')==text)]
    assert len(matches)==1,(kind,matches)
    return list(map(int,re.findall(r'\d+',matches[0]['bounds'])))
x1,y1,x2,y2=bounds('XComponent');sx1,sy1,sx2,sy2=bounds('Button','开始')
x=(x1+x2)//2;y=(y1+y2)//2
m.click(h,'开始')
command=['shell','uinput','-T']
for _ in range(35):
    command+=['-d',str(x),str(y),'-i','25','-u',str(x),str(y),'-i','25']
output=m.run(h,*command)
assert 'parameter error' not in output,output
# Independent two-pointer stream through the same native XComponent handler.
print(m.run(h,'shell','uinput','-T','-m',str(x-200),str(y),str(x-100),str(y+80),str(x+200),str(y),str(x+100),str(y+80),'300'))
time.sleep(8)
m.export(h,prefix+'-input');m.snapshot(h,prefix+'-input-after')
b=(m.OUT/(prefix+'-input.replay')).read_bytes();events=[struct.unpack_from('<qIIff',b,i) for i in range(32,len(b),24)]
r=json.loads((m.OUT/(prefix+'-input-result.json')).read_text())
head=subprocess.check_output([str(m.ROOT/'build/host/Debug/hrk_headless.exe'),'tests/data/charts/simple_sequence.json','tests/data/audio/short_test.wav',str(m.OUT/(prefix+'-input.replay'))],cwd=m.ROOT,text=True)
(m.OUT/(prefix+'-input-headless.json')).write_text(head)
summary={'perfect':r['perfect'],'events':len(events),'pointers':sorted(set(e[1] for e in events)),'phases':sorted(set(e[2] for e in events)),'exactHeadless':json.loads(head)==r}
(m.OUT/(prefix+'-input-assertions.json')).write_text(json.dumps(summary,indent=2))
print(summary)
assert r['perfect']>0 and summary['exactHeadless']
assert len(summary['pointers'])>=2 and set(summary['phases'])>={0,1,2}
