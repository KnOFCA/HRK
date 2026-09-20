import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
function files(dir){return fs.readdirSync(dir,{withFileTypes:true}).flatMap(e=>e.isDirectory()?files(path.join(dir,e.name)):/\.(cpp|h)$/.test(e.name)?[path.join(dir,e.name)]:[]);}
const rules=[['TC-ARCH-001','src/kernel',/games\/|\b(pjsk|arcaea|phigros|mania|tap|flick|slide|arc|judgeline|lane)\b/i],['TC-ARCH-002','src/kernel',/ohaudio|native_window|xcomponent|\bGLES|\bEGL|\bnapi/i],['TC-ARCH-003','src/games',/#\s*include\s*["<](?:platform\/|.*(?:GLES|EGL|ohaudio|xcomponent))/i],['TC-ARCH-004','src/platform',/#\s*include\s*["<]games\//],['TC-ARCH-005','src',/#\s*include\s*["<]miniaudio\.h/],['TC-REN-002','src/games',/#\s*include\s*["<](?:GLES|EGL)/]];
let failures=0;
for(const [id,dir,pattern] of rules){const list=files(path.join(root,dir));const bad=list.filter(f=>(id!=='TC-ARCH-005'||f.endsWith('.h'))&&pattern.test(fs.readFileSync(f,'utf8')));if(!list.length||bad.length){console.error(`${id} FAIL ${bad.join(', ')}`);failures++;}else console.log(`${id} PASS (${list.length} files)`);}
const cmake=fs.readFileSync(path.join(root,'CMakeLists.txt'),'utf8');
for(const [target,forbidden] of [['hrk_base',/hrk_(?:time|game|platform|runtime)/],['hrk_game_reference',/hrk_platform/],['hrk_platform_headless',/hrk_game/],['hrk_platform_harmony',/hrk_game/]]){
 const line=cmake.split('\n').find(l=>l.startsWith(`hrk_library(${target} `)||l.trimStart().startsWith(`hrk_library(${target} `));
 if(!line||forbidden.test(line.replace(target,''))){console.error(`TARGET FAIL ${target}`);failures++;}
}
console.log(`Architecture: ${failures?'FAIL':'PASS'}`);process.exitCode=failures?1:0;
