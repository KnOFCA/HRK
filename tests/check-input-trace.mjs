// Independent exporter contract checks. The TASK-003 ingestion CLI is separate.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import os from 'node:os';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const [binary,base]=process.argv.slice(2);
fs.mkdirSync(base,{recursive:true});
const directory=fs.mkdtempSync(path.join(base,'run-'));
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
const chart=Buffer.from('{"bpm":120,"notes":[{"beat":2,"x":0.5}]}');
const audio=Buffer.alloc(44+48000*2*8*2);
audio.write('RIFF');audio.writeUInt32LE(audio.length-8,4);audio.write('WAVEfmt ',8);audio.writeUInt32LE(16,16);audio.writeUInt16LE(1,20);audio.writeUInt16LE(2,22);audio.writeUInt32LE(48000,24);audio.writeUInt32LE(192000,28);audio.writeUInt16LE(4,32);audio.writeUInt16LE(16,34);audio.write('data',36);audio.writeUInt32LE(audio.length-44,40);
fs.writeFileSync(path.join(directory,'chart.json'),chart);fs.writeFileSync(path.join(directory,'audio.wav'),audio);
const sourceFiles=dir=>fs.readdirSync(dir,{withFileTypes:true}).flatMap(e=>e.isDirectory()?sourceFiles(path.join(dir,e.name)):/\.(cpp|h)$/.test(e.name)?[path.join(dir,e.name)]:[]);
const sourceHash=createHash('sha256');for(const file of sourceFiles(path.join(root,'src')).sort()){sourceHash.update(path.relative(root,file).replaceAll('\\','/'));sourceHash.update(fs.readFileSync(file));}
const env={...process.env,HRK_TRACE_RUN_ID:path.basename(directory),HRK_TRACE_SOURCE:`sha256:${sourceHash.digest('hex')}`,HRK_TRACE_SPEC:`sha256:${sha(fs.readFileSync(path.join(root,'specs/product/input-reliability/spec.md')))}`,HRK_TRACE_MACHINE:os.hostname(),HRK_TRACE_OS:`${os.type()} ${os.release()}`,HRK_TRACE_CHART:sha(chart),HRK_TRACE_AUDIO:sha(audio)};
const run=spawnSync(binary,[directory],{encoding:'utf8',timeout:120000,env});
process.stdout.write(run.stdout??'');process.stderr.write(run.stderr??'');
if(run.error)throw run.error;
assert.equal(run.status,0,'C++ capture tests');
const common=['kind','recordSequence'];
const runtime=['source','sourceSequence','epoch'];
const fields={
 header:['traceVersion','runId','provenance','captureConfig'],
 input:[...runtime,'eventSequence','batchSequence','pointIndex','pointerId','phase','position','rawHostTime','receiveHostTime','consumeHostTime','mappedSongTime','mappingSampleSequence','watermarkBefore','watermarkAfter','songNow','stage','disposition','error','reason','rawPhase','actionSequence','controlSequence','updateSequence'],
 clock:[...runtime,'sampleHostTime','sampleSongTime','sampleSuccess','query','caller','actionSequence','queryIndex','positionResult'],
 control:[...runtime,'operation','hostTime','targetSongTime','result','actionSequence','boundary','activePointers'],
 update:[...runtime,'updateSequence','hostTime','watermarkBefore','watermarkAfter','songNow','actionSequence','boundary','result','activePointers'],
 summary:['countsByPhaseAndReason','captureDropped','pendingAtEnd','complete','ingressCount','acceptedCount','rejectedCount','clearedCount','captureDroppedBySource','platformQueueDropped','sessionQueueDropped','pendingEventSequences','incompleteReasons']
};
const u64=v=>typeof v==='string' && /^(0|[1-9][0-9]*)$/.test(v) && BigInt(v)<=18446744073709551615n;
const i64=v=>typeof v==='string' && /^(0|-?[1-9][0-9]*)$/.test(v) && BigInt(v)>=-9223372036854775808n && BigInt(v)<=9223372036854775807n;
const keys=(v,expected)=>assert.deepEqual(Object.keys(v).sort(),[...expected].sort());
let files=0;
for(const name of fs.readdirSync(directory).filter(n=>n.endsWith('.jsonl'))){
 const text=fs.readFileSync(path.join(directory,name),'utf8');assert(!text.startsWith('\ufeff'));
 const lines=text.trimEnd().split('\n');assert(lines.every(l=>Buffer.byteLength(l)<=1024*1024));
 const records=lines.map(l=>JSON.parse(l));const header=records[0],summary=records.at(-1);
 assert.equal(header.kind,'header');assert.equal(header.traceVersion,1);assert.equal(summary.kind,'summary');
 keys(header.provenance,['sourceRevision','specRevision','policyVersion','buildMode','deviceModel','osVersion','actualRefreshHz','chartSha256','audioSha256','origin']);
 keys(header.captureConfig,['enabled','platformCapacity','sessionCapacity','slotBytes','memoryBudgetBytes','hostClock','timeUnit','inputDeliveryGrace','policy']);
 assert.equal(header.captureConfig.platformCapacity,65536);assert.equal(header.captureConfig.sessionCapacity,65536);assert(header.captureConfig.slotBytes<=256);assert.equal(header.captureConfig.memoryBudgetBytes,34603008);
 const byId=new Map();const totals=new Map();const stack=[];let lastAction=0n;const queries=new Map();let sources={platform:0n,session:0n};let sessionSeen=false;
 for(const [index,r] of records.entries()){
  keys(r,[...common,...fields[r.kind]]);assert.equal(r.recordSequence,String(index+1));
  if(r.source){assert(['platform','session'].includes(r.source));assert(u64(r.epoch));assert.equal(r.sourceSequence,String(++sources[r.source]));if(r.source==='session')sessionSeen=true;else assert(!sessionSeen);}
  for(const k of ['eventSequence','batchSequence','mappingSampleSequence','actionSequence','controlSequence','updateSequence'])if(k in r && r[k]!==null)assert(u64(r[k]),`${name} ${k}`);
  for(const k of ['rawHostTime','receiveHostTime','consumeHostTime','mappedSongTime','watermarkBefore','watermarkAfter','songNow','sampleHostTime','sampleSongTime','positionResult','hostTime','targetSongTime'])if(k in r && r[k]!==null)assert(i64(r[k]),`${name} ${k}`);
  if(r.source==='session' && summary.complete){assert(r.actionSequence!==null);const a=BigInt(r.actionSequence);assert(a>=lastAction && a<=lastAction+1n,`${name} record ${r.recordSequence}: action ${a} after ${lastAction}`);lastAction=a;}
  if(r.kind==='clock'){
   assert(['position','clockSample','anchor'].includes(r.query));assert(['start','resume','seek','update','render','status','pause'].includes(r.caller));
   if(summary.complete){const next=queries.get(r.actionSequence)??0;assert.equal(r.queryIndex,next);queries.set(r.actionSequence,next+1);}
   if(r.query==='position'){assert(r.sampleSuccess);assert.equal(r.sampleHostTime,null);assert.equal(r.sampleSongTime,null);assert.notEqual(r.positionResult,null);}
   else{assert.equal(r.positionResult,null);assert.equal(r.sampleHostTime!==null,r.sampleSuccess);assert.equal(r.sampleSongTime!==null,r.sampleSuccess);}
  }
  if(r.kind==='control' || r.kind==='update'){
   if(r.boundary==='begin'){assert.equal(r.result,null);assert.equal(r.activePointers,null);stack.push(r);}
   else{assert(Array.isArray(r.activePointers));assert.deepEqual(r.activePointers,[...new Set(r.activePointers)].sort((a,b)=>a-b));assert.notEqual(r.result,null);if(summary.complete){const b=stack.pop();assert(b);assert.equal(b.actionSequence,r.actionSequence);assert.equal(b.kind,r.kind);assert.equal(b.operation,r.operation);assert.equal(b.updateSequence,r.updateSequence);}}
  }
  if(r.kind!=='input')continue;
  keys(r.position,['x','y']);assert.equal(r.phase,['DOWN','MOVE','UP','CANCEL'][r.rawPhase]??'UNKNOWN');assert.equal(r.batchSequence===null,r.pointIndex===null);
  assert(['receive','platform_enqueue','platform_poll','session_enqueue','map','pending','terminal'].includes(r.stage));
  const expectedDisposition={receive:'observed',platform_enqueue:'queued',platform_poll:'observed',session_enqueue:'queued',map:'observed',pending:'pending'};
  if(r.stage!=='terminal')assert.equal(r.disposition,expectedDisposition[r.stage]);else assert(['accepted','rejected','cleared'].includes(r.disposition));
  if(r.source==='platform')for(const k of ['actionSequence','controlSequence','updateSequence','consumeHostTime','mappedSongTime','mappingSampleSequence','watermarkBefore','watermarkAfter','songNow'])assert.equal(r[k],null);
  if(!summary.complete)continue;
  if(r.stage==='receive'){assert(!byId.has(r.eventSequence));byId.set(r.eventSequence,{raw:r,terminal:null,mapped:null});}
  const e=byId.get(r.eventSequence);assert(e);
  for(const k of ['batchSequence','pointIndex','pointerId','phase','rawPhase','position','rawHostTime','receiveHostTime'])assert.deepEqual(r[k],e.raw[k]);
  assert.equal(e.terminal,null,'stage after terminal');
  if(r.mappingSampleSequence!==null){const c=records[Number(r.mappingSampleSequence)-1];assert.equal(c.kind,'clock');assert(c.sampleSuccess && c.query!=='position');if(e.mapped)for(const k of ['mappedSongTime','mappingSampleSequence','consumeHostTime'])assert.equal(r[k],e.mapped[k]);else e.mapped=r;}
  if(r.controlSequence!==null){const c=records[Number(r.controlSequence)-1];assert.equal(c.kind,'control');assert.equal(c.boundary,'begin');assert.equal(c.actionSequence,r.actionSequence);}
  if(r.stage==='terminal'){e.terminal=r;const key=JSON.stringify([r.phase,r.disposition,r.reason]);totals.set(key,(totals.get(key)??0n)+1n);}
 }
 for(const k of ['captureDropped','pendingAtEnd','ingressCount','acceptedCount','rejectedCount','clearedCount','platformQueueDropped','sessionQueueDropped'])assert(u64(summary[k]));
 assert.equal(BigInt(summary.captureDropped),BigInt(summary.captureDroppedBySource.platform)+BigInt(summary.captureDroppedBySource.session));
 if(summary.complete){
  assert.equal(stack.length,0);assert.equal(summary.captureDropped,'0');assert.equal(summary.incompleteReasons.length,0);
  assert.equal(BigInt(summary.ingressCount),BigInt(summary.acceptedCount)+BigInt(summary.rejectedCount)+BigInt(summary.clearedCount)+BigInt(summary.pendingAtEnd));
  assert.equal(summary.ingressCount,String(byId.size));const pending=[...byId].filter(([,e])=>!e.terminal).map(([id])=>id);
  assert.deepEqual(summary.pendingEventSequences,pending.sort((a,b)=>Number(BigInt(a)-BigInt(b))));assert.equal(summary.pendingAtEnd,String(pending.length));
  const counts=new Map(summary.countsByPhaseAndReason.map(c=>{keys(c,['phase','disposition','reason','count']);assert(u64(c.count));return [JSON.stringify([c.phase,c.disposition,c.reason]),BigInt(c.count)];}));assert.deepEqual(counts,totals);
 }else assert(summary.incompleteReasons.length>0);
 ++files;
}
assert(files>=14);
console.log(`Exporter schema/references/accounting: ${files} files PASS`);
console.log(`Artifacts: ${directory}`);
