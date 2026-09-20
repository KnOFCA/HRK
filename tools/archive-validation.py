"""Archive an actually executed run; never infer device PASS from source or build success."""
import argparse,datetime,hashlib,json,platform,re,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def digest(paths):
    return 'sha256:'+hashlib.sha256(''.join(f'{p.relative_to(ROOT).as_posix()}\0{sha(p)}\n' for p in sorted(paths)).encode()).hexdigest()
def command(args):
    result=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace')
    return {'argv':args,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr}
parser=argparse.ArgumentParser();parser.add_argument('--device-results',required=True);parser.add_argument('--run-id',required=True);args=parser.parse_args()
output=ROOT/'evidence/PROD-HRK'/args.run_id;output.mkdir(parents=True,exist_ok=False)
runs=[command(['build/host/Debug/hrk_tests.exe']),command(['node','tools/check-architecture.mjs']),command(['node','tools/check-headless.mjs',str(ROOT/'build/host/Debug/hrk_headless.exe')])]
if any(r['exit_code'] for r in runs):raise SystemExit('FAIL: host verification; inspect output before archiving')
results={}
for run in runs:
    for case in re.findall(r'^(TC-[A-Z0-9-]+-\d{3}) PASS',run['stdout'],re.M):results[case]={'status':'PASS','method':'automated host','evidence':'host-results.json'}
device=json.loads((ROOT/args.device_results).read_text(encoding='utf-8'))
for key,value in device['cases'].items():
    if key in results:raise SystemExit('Duplicate result: '+key)
    if value['status'] not in ('PASS','FAIL','BLOCKED','UNSUPPORTED_DEVICE'):raise SystemExit('Invalid status: '+key)
    results[key]=value
spec=ROOT/'specs/product/rhythm-kernel/spec.md';acceptance=spec.with_name('acceptance.md')
all_cases=re.findall(r'^## (TC-[A-Z0-9-]+-\d{3})',acceptance.read_text(encoding='utf-8'),re.M)
missing=set(all_cases)-set(results)
if missing:raise SystemExit('Missing acceptance results: '+','.join(sorted(missing)))
if set(results)-set(all_cases):raise SystemExit('Unknown acceptance IDs')
(output/'host-results.json').write_text(json.dumps(runs,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(output/'acceptance-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
artifacts=[]
for rel in device['artifacts']:
    p=ROOT/rel
    if not p.is_file():raise SystemExit('Missing artifact: '+rel)
    artifacts.append({'path':rel,'sha256':sha(p),'bytes':p.stat().st_size})
    if p.suffix in ('.json','.replay','.txt','.png'):
        artifacts[-1]['archived_path']=p.name
        shutil.copyfile(p,output/p.name)
# Use git's ignore rules to exclude local signing settings, build trees, and credentials.
listing=command(['git','ls-files','--cached','--others','--exclude-standard'])
tracked=[ROOT/p for p in set(listing['stdout'].splitlines()) if (ROOT/p).is_file()]
source=[p for p in tracked if p.relative_to(ROOT).parts[0]=='src' or p.name=='CMakeLists.txt']
validators=[p for p in tracked if p.relative_to(ROOT).parts[0] in ('tools','tests')]
file_hashes={p.relative_to(ROOT).as_posix():sha(p) for p in source}
(output/'source-hashes.json').write_text(json.dumps(file_hashes,indent=2)+'\n')
statuses={v['status'] for v in results.values()}
overall='FAIL' if 'FAIL' in statuses else 'BLOCKED' if 'BLOCKED' in statuses else 'PASS'
manifest={'feature_id':'PROD-HRK','run_id':args.run_id,'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'environment':{'host':platform.platform(),**device['environment']},'specification_revision':digest([spec,acceptance]),'design_revision':digest([spec.with_name('design.md'),ROOT/'DESIGN.md']),'implementation_commit':None,'implementation_revision':digest(source),'validator_commit':None,'validator_revision':digest(validators),'validation_commit':None,'validation_ref':None,'revision_note':'Uncommitted working tree; exact content hashes recorded. No commit or release tag claimed. Local signing material is intentionally excluded.','commands':[{'argv':r['argv'],'exit_code':r['exit_code']} for r in runs]+device['commands'],'acceptance_results':'acceptance-results.json','overall':overall,'artifacts':artifacts,'notes':device.get('notes',[])}
groups = {
 'PROD-HRK-AC-001':['BUILD','ARCH','HEAD','BACKEND'],
 'PROD-HRK-AC-002':['BASE','TIME','TL'],
 'PROD-HRK-AC-003':['AUDIO','IN','RT'],
 'PROD-HRK-AC-004':['GAME','CHART','JUDGE','SCORE','AUTO'],
 'PROD-HRK-AC-005':['REP','DET'],
 'PROD-HRK-AC-006':['REN','HAR','E2E','LIFE'],
 'PROD-HRK-AC-007':['RES','DIAG','STAB','MEM','PERF','ERR']}
manifest['acceptance_criteria']={}
for ac,prefixes in groups.items():
    ids=[case for case in results if any(case.startswith('TC-'+prefix+'-') for prefix in prefixes)]
    states={results[case]['status'] for case in ids}
    status='FAIL' if 'FAIL' in states else 'BLOCKED' if 'BLOCKED' in states else 'PASS'
    if ac=='PROD-HRK-AC-007':status=overall
    manifest['acceptance_criteria'][ac]={'status':status,'cases':ids}
(output/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(overall,len(results),'acceptance cases archived:',output)
