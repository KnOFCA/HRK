"""HDC helpers for the HRK app. Every click is resolved from an observed layout."""
import argparse, json, re, subprocess, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/device'
BUNDLE='com.hrk.reference'

def run(hdc,*args):
    p=subprocess.run([hdc,*args],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30)
    if p.returncode or '[Fail]' in p.stdout or 'error:' in p.stdout.lower():
        raise RuntimeError(p.stdout+p.stderr)
    return p.stdout.strip()

def snapshot(hdc,name):
    OUT.mkdir(parents=True,exist_ok=True)
    run(hdc,'shell','uitest','dumpLayout','-b',BUNDLE,'-p','/data/local/tmp/hrk-layout.json')
    run(hdc,'file','recv','/data/local/tmp/hrk-layout.json',str(OUT/(name+'-layout.json')))
    tree=json.loads((OUT/(name+'-layout.json')).read_text(encoding='utf-8'))
    nodes=[]
    def walk(v):
        if isinstance(v,dict):
            if 'attributes' in v:nodes.append(v['attributes'])
            for child in v.values():walk(child)
        elif isinstance(v,list):
            for child in v:walk(child)
    walk(tree)
    return nodes

def click(hdc,text):
    nodes=snapshot(hdc,'before-click')
    matches=[n for n in nodes if n.get('type')=='Button' and n.get('text')==text]
    if len(matches)!=1 or matches[0].get('enabled')!='true':raise RuntimeError('Button unavailable: '+text)
    x1,y1,x2,y2=map(int,re.findall(r'\d+',matches[0]['bounds']))
    run(hdc,'shell','uitest','uiInput','click',str((x1+x2)//2),str((y1+y2)//2))

def export(hdc,name):
    click(hdc,'保存 Replay');time.sleep(.3)
    for remote,suffix in [('session.replay','.replay'),('session-result.json','-result.json'),('frame-metrics.json','-metrics.json')]:
        run(hdc,'file','recv','-b',BUNDLE,'/data/storage/el2/base/haps/entry/files/'+remote,str(OUT/(name+suffix)))
    result=json.loads((OUT/(name+'-result.json')).read_text())
    print(json.dumps({k:result[k] for k in ['score','perfect','miss']} | {'judgmentCount':len(result['judgments'])}))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--hdc',required=True);parser.add_argument('action',choices=['snapshot','click','export']);parser.add_argument('value');args=parser.parse_args()
    if args.action=='snapshot':
        nodes=snapshot(args.hdc,args.value)
        for node in nodes:
            if node.get('text'):print(json.dumps({'text':node['text'],'bounds':node.get('bounds')},ensure_ascii=True))
    elif args.action=='click':click(args.hdc,args.value)
    else:export(args.hdc,args.value)
