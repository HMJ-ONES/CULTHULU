// Node 24+, no packages/downloads. Exercises exact functions reconstructed from
// the reviewed backend patch, not an independently rewritten implementation.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {stripTypeScriptTypes} from 'node:module';
const patch=fs.readFileSync(new URL('../Docs/CloudLobbyServerSafety.patch',import.meta.url),'utf8');
let old='',updated='',hunk=false;
for(const line of patch.split(/\r?\n/)) {
 if(line.startsWith('@@')) {hunk=true;continue;}
 if(!hunk) continue;
 if(line.startsWith('-')) old+=line.slice(1)+'\n';
 else if(line.startsWith('+')) updated+=line.slice(1)+'\n';
 else if(line.startsWith(' ')) {old+=line.slice(1)+'\n';updated+=line.slice(1)+'\n';}
}
function fn(text,marker) {
 const start=text.indexOf(marker);assert.ok(start>=0);
 const body=text.indexOf('{',start);let depth=0;
 for(let i=body;i<text.length;i++) {if(text[i]==='{')depth++;if(text[i]==='}'&&--depth===0)return text.slice(start,i+1);}
 throw new Error('Incomplete function in server patch');
}
const functions=fn(updated,'function encKey(')+'\n'+fn(updated,'async function reserveSlot(')+'\n'+fn(old,'async function reserveSlot(').replace('async function reserveSlot(','async function originalReserveSlot(');
const setup=`
type ApiEnv=any;type LobbyRow=any;type Reservation=any;type ConnectionDescriptor=any;
class HttpError extends Error {
 status:number;code:string;
 constructor(status:number,code:string,message:string){super(message);this.status=status;this.code=code;}
}
let badDecrypt=false,sequence=0;
const RESERVATION_TTL_SEC=120;
const randomToken=()=>"test-only-token";
const randomId=(prefix:string)=>prefix+"_"+(++sequence);
const sha256=async(s:string)=>s;
const buildConnect=(c:any)=>c;
const launchUri=(...args:any[])=>"test-only-uri";
async function decryptJson<T>(data:any,key:string):Promise<T>{if(badDecrypt)throw new Error("test decrypt failure");return {} as T;}
function makeStore(insertFails=false,logFails=false){
 return {kind:"d1",occupied:1,inserted:false,released:false,
 async activeReservationFor(){return null;},
 async tryOccupySlot(){this.occupied++;return true;},
 async releaseSlot(){this.occupied--;},
 async insertReservation(){if(insertFails)throw new Error("test insert failure");this.inserted=true;},
 async setReservationStatus(){this.released=true;},
 async logEvent(){if(logFails)throw new Error("test log failure");}
 };
}
`;
const tests=`
const lobby={id:"lby_test",connectionEnc:"test-only-cipher",transport:"radmin_desktop"};
assert.throws(()=>encKey({store:{kind:"d1"}}),(error:any)=>error.status===503);
assert.equal(encKey({store:{kind:"d1"},CONNECTION_ENCRYPTION_KEY:"stable-test-key"}),"stable-test-key");
assert.ok(encKey({store:{kind:"memory"}}));
badDecrypt=true;
let s=makeStore();await assert.rejects(()=>originalReserveSlot({store:s,CONNECTION_ENCRYPTION_KEY:"stable-test-key"},lobby,"player",100),/test decrypt failure/);assert.equal(s.occupied,2);
s=makeStore();await assert.rejects(()=>reserveSlot({store:s,CONNECTION_ENCRYPTION_KEY:"stable-test-key"},lobby,"player",100),/test decrypt failure/);assert.equal(s.occupied,1);assert.equal(s.inserted,false);
badDecrypt=false;
s=makeStore(true);await assert.rejects(()=>reserveSlot({store:s,CONNECTION_ENCRYPTION_KEY:"stable-test-key"},lobby,"player",100),/test insert failure/);assert.equal(s.occupied,1);
s=makeStore(false,true);await assert.rejects(()=>reserveSlot({store:s,CONNECTION_ENCRYPTION_KEY:"stable-test-key"},lobby,"player",100),/test log failure/);assert.equal(s.occupied,1);assert.equal(s.released,true);
s=makeStore();const r=await reserveSlot({store:s,CONNECTION_ENCRYPTION_KEY:"stable-test-key"},lobby,"player",100);assert.equal(s.occupied,2);assert.ok(r.id);assert.equal(r.lobbyId,"lby_test");
`;
const AsyncFunction=Object.getPrototypeOf(async function(){}).constructor;
await new AsyncFunction('assert',stripTypeScriptTypes(setup+'\n'+functions+'\n'+tests))(assert);
console.log(JSON.stringify({passed:true,checks:7,scope:'exact patched functions; mocked crypto/store; production deployment not tested'}));
