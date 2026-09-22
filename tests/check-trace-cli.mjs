import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
const [binary,capture,base]=process.argv.slice(2);
fs.mkdirSync(base,{recursive:true});
const run=spawnSync(process.execPath,['tests/check-input-trace.mjs',capture,base],{encoding:'utf8',timeout:180000});
assert.equal(run.status,0,run.stdout+run.stderr);
const directory=run.stdout.match(/Artifacts: (.+)/)[1].trim();
const outputs=fs.mkdtempSync(path.join(base,'cli-'));
let checks=0,index=0;
const invoke=(command,args,expected=0,error)=>{
 const out=path.join(outputs,`${index++}.json`);
 const result=spawnSync(binary,[command,...args,...(command==='minimize'?[]:['--out',out])],{encoding:'utf8',timeout:90000});
 assert.equal(result.status,expected,`${command} ${args.join(' ')}: ${result.error??''} ${result.stderr}`);
 const data=command==='minimize'?null:JSON.parse(fs.readFileSync(out,'utf8'));
 if(error)assert.equal(data?.error??JSON.parse(result.stderr).error,error,result.stderr);
 ++checks;return {data,out};
};
const trace=n=>path.join(directory,n);
const assets=['--chart',trace('chart.json'),'--audio',trace('audio.wav')];
const replay=n=>invoke('replay',['--trace',trace(n),...assets]);
const parse=n=>fs.readFileSync(trace(n),'utf8').trimEnd().split('\n').map(JSON.parse);
const write=records=>{const p=path.join(outputs,`input-${index++}.jsonl`);fs.writeFileSync(p,typeof records==='string'?records:records.map(r=>JSON.stringify(r)).join('\n')+'\n');return p;};
const bad=(mutate,error='TraceInvalidSchema',command='validate')=>{const records=parse('investigation-0-original.jsonl');mutate(records);return invoke(command,['--trace',write(records),...(command==='replay'?assets:[])],2,error);};
const results={};
for(const name of fs.readdirSync(directory).filter(n=>n.endsWith('.jsonl'))){
 const records=parse(name);
 if(records.at(-1).complete){invoke('validate',['--trace',trace(name)]);results[name]=replay(name);}
 else if(records.length<1000)invoke('validate',['--trace',trace(name)],2,'TraceIncomplete');
}
for(let scenario=0;scenario<5;++scenario){
 const a=results[`investigation-${scenario}-original.jsonl`],b=results[`investigation-${scenario}-control.jsonl`];
 invoke('compare',['--expected',a.out,'--actual',a.out]);
 const diff=invoke('compare',['--expected',a.out,'--actual',b.out],1).data;
 assert.equal(diff.equal,false);assert(diff.path.startsWith('$.'));assert('expected' in diff&&'actual' in diff);
 const again=replay(`investigation-${scenario}-original.jsonl`);assert.deepEqual(again.data,a.data);
}
assert.equal(results['investigation-0-original.jsonl'].data.decisions.filter(d=>d.reason==='before_watermark').length,1);
assert.equal(results['investigation-0-control.jsonl'].data.decisions.filter(d=>d.reason==='before_watermark').length,0);
assert.equal(results['investigation-1-original.jsonl'].data.score.miss,1);
assert.equal(results['investigation-1-control.jsonl'].data.score.perfect,1);
assert.equal(results['investigation-4-original.jsonl'].data.score.miss,1);
assert.equal(results['investigation-4-control.jsonl'].data.score.perfect,1);
bad(r=>r[0].traceVersion=2,'TraceUnsupportedVersion');
bad(r=>r[0].unknown=true);
bad(r=>r[1].epoch='18446744073709551616','TraceInvalidValue');
bad(r=>r[1].sourceSequence='01','TraceInvalidValue');
bad(r=>r.find(x=>x.kind==='input').rawHostTime='-9223372036854775809','TraceInvalidValue');
bad(r=>r.find(x=>x.kind==='input').rawHostTime='-0','TraceInvalidValue');
bad(r=>r.find(x=>x.kind==='input').rawHostTime=null);
bad(r=>r.find(x=>x.kind==='input').phase='TYPO','TraceInvalidValue');
bad(r=>r.find(x=>x.kind==='input').position.x=null,'TraceInvalidValue');
bad(r=>r.find(x=>x.mappingSampleSequence!=null).mappingSampleSequence='18446744073709551615','TraceMissingReference');
bad(r=>r.find(x=>x.query==='position').queryIndex=1,'TraceQueryMismatch');
bad(r=>r.find(x=>x.query==='position').sampleSuccess=false);
bad(r=>r.at(-1).acceptedCount='42','TraceIncomplete');
bad(r=>r.at(-1).pendingEventSequences=['1','1'],'TraceInvalidValue');
bad(r=>r.find(x=>x.kind==='control'&&x.boundary==='end').activePointers=[2,1],'TraceInvalidValue');
bad(r=>r.find(x=>x.kind==='input'&&x.stage==='map').stage='platform_poll','TraceIncomplete');
bad(r=>{for(const x of r)if(x.kind==='input'&&x.eventSequence==='2')x.pointIndex=0;},'TraceIncomplete');
bad(r=>r.pop(),'TraceIncomplete');
bad(r=>r[0].provenance.chartSha256='0'.repeat(64),'TraceAssetMismatch','replay');
bad(r=>{for(const x of r)if(x.source==='platform')x.epoch='999';},'TraceScheduleAmbiguous','replay');
bad(r=>r.find(x=>x.query==='clockSample').query='position');
const raw=fs.readFileSync(trace('empty.jsonl'),'utf8');
for(const invalid of ['', '\ufeff'+raw,raw+'\n',raw.slice(0,-6),raw.replace('"traceVersion":1','"traceVersion":1,"traceVersion":1'),raw.replace('"traceVersion":1','"traceVersion":NaN'),raw.replace('"traceVersion":1','"traceVersion":1e999')])invoke('validate',['--trace',write(invalid)],2);
invoke('validate',['--trace',write(' '.repeat(1024*1024+1))],2,'TraceLimitExceeded');
const utf=path.join(outputs,'invalid-utf.jsonl');fs.writeFileSync(utf,Buffer.from([0x7b,0x22,0xc0,0xaf,0x22,0x3a,0x31,0x7d]));invoke('validate',['--trace',utf],2,'TraceInvalidSchema');
// i64 extremes are evidence, not necessarily legal gameplay input. Keep all stages identical.
for(const value of ['-9223372036854775808','9223372036854775807']){const r=parse('investigation-0-original.jsonl');for(const record of r)if(record.kind==='input'&&record.eventSequence==='1')record.rawHostTime=value;invoke('validate',['--trace',write(r)]);}
// Public query shape remains valid while the call type changes: runner must detect it.
bad(r=>{const q=r.find(x=>x.query==='clockSample');q.query='position';q.sampleSuccess=true;q.sampleHostTime=null;q.sampleSongTime=null;q.positionResult='0';},'TraceQueryMismatch','replay');
const noOverwrite=results['empty.jsonl'].out,original=fs.readFileSync(noOverwrite);
const overwrite=spawnSync(binary,['validate','--trace',trace('empty.jsonl'),'--out',noOverwrite],{encoding:'utf8'});assert.equal(overwrite.status,3);assert.deepEqual(fs.readFileSync(noOverwrite),original);++checks;
invoke('validate',['--trace',path.join(outputs,'missing')],3,'TraceIoError');
const brokenOut=spawnSync(binary,['validate','--trace',trace('empty.jsonl'),'--out',path.join(outputs,'absent','result.json')],{encoding:'utf8'});assert.equal(brokenOut.status,3);assert(!fs.existsSync(path.join(outputs,'absent')));++checks;
const malformedResult=path.join(outputs,'malformed-result.json');fs.writeFileSync(malformedResult,'{}');invoke('compare',['--expected',malformedResult,'--actual',results['empty.jsonl'].out],2,'TraceInvalidSchema');
const mini=path.join(outputs,'minimal');
invoke('minimize',['--trace',trace('investigation-0-original.jsonl'),...assets,'--event','2','--out-dir',mini]);
const meta=JSON.parse(fs.readFileSync(path.join(mini,'minimization.json')));assert.equal(meta.targetEventSequence,'2');assert(meta.retainedEventSequences.includes('2'));assert(meta.steps.length>0);
const minResult=invoke('replay',['--trace',path.join(mini,'trace.jsonl'),...assets]).data;
assert(minResult.decisions.some(d=>d.eventSequence==='2'&&d.reason==='before_watermark'));
invoke('minimize',['--trace',trace('empty.jsonl'),...assets,'--event','1','--out-dir',path.join(outputs,'no-target')],2,'TraceTargetNotReproduced');
assert(!fs.existsSync(path.join(outputs,'no-target')));
console.log(`Trace CLI: ${checks} checks PASS; five original/control pairs; minimization replay PASS`);
console.log(`Capture artifacts: ${directory}`);
console.log(`CLI artifacts: ${outputs}`);
