// Synthetic messages only. No real CAPTCHA is solved or submitted.
const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const {webcrypto}=require('crypto'),{spawn}=require('child_process');
const html=fs.readFileSync(path.join(__dirname,'../docs/verify.html'),'utf8');
const script=html.match(/<script>\s*([\s\S]*?)<\/script>/)[1];
const state='a'.repeat(64),token='synthetic/solution+';
function page(pub,back='http://192.168.1.2:12345/captcha-return',extra={}){
 let callback,clicked,form,cleared=false,notice='',poll,response='';
 const button={disabled:true,addEventListener:(event,fn)=>clicked=fn};
 const c={URL,URLSearchParams,Uint8Array,TextEncoder,atob,btoa,crypto:webcrypto,setInterval:fn=>{poll=fn;return 1;},clearInterval:()=>{poll=null;},
  location:{hash:'#'+new URLSearchParams({v:'1',limit:'4095',state,sitekey:'test-site',rqdata:'test+data',pub,return:back,...extra}),pathname:'/DiscordNX2/verify.html'},
  history:{replaceState:()=>cleared=true},document:{getElementById:id=>id==='send'?button:{set textContent(v){notice=v;}},
   createElement:tag=>({tag,children:[],appendChild(x){this.children.push(x);},submit(){form=this;}}),body:{appendChild(){}}},
  hcaptcha:{getResponse:()=>response,render:(id,o)=>{callback=o.callback;return 7;},setData:(id,data)=>assert.equal(data.rqdata,'test+data')}};
 vm.createContext(c);vm.runInContext(script,c);vm.runInContext('ready()',c);
 return{get callback(){return callback;},get notice(){return notice;},get cleared(){return cleared;},get form(){return form;},button,click:()=>clicked(),pollWith:token=>{response=token;poll();}};
}
function lines(child){let buffer='',queue=[],waiters=[];child.stdout.on('data',data=>{buffer+=data;let i;while((i=buffer.indexOf('\n'))>=0){const line=buffer.slice(0,i).trim();buffer=buffer.slice(i+1);if(waiters.length)waiters.shift()(line);else queue.push(line);}});return()=>queue.length?Promise.resolve(queue.shift()):new Promise(resolve=>waiters.push(resolve));}
async function main(){
 const child=spawn('wsl',['-d','Debian','--','/var/tmp/discordnx2-phone-crypto-test'],{stdio:['pipe','pipe','inherit']});
 const next=lines(child);const timeout=setTimeout(()=>{child.kill();throw Error('crypto test timed out');},30000);
 try{
  const pub=await next();assert(pub.startsWith('MIIB'));
  const p=page(pub);assert(p.cleared&&p.callback&&p.button.disabled);
  await p.callback(token);assert(!p.button.disabled);p.click();
  assert.equal(p.form.method,'POST');assert.equal(p.form.action,'http://192.168.1.2:12345/captcha-return');
  const payload=p.form.children[0].value;assert(/^[A-Za-z0-9_-]+$/.test(payload));assert(!payload.includes(token));
  async function native(value,s=state,want=token){child.stdin.write(s+'\n'+value+'\n'+want+'\n');return next();}
  assert.equal(await native(payload),'OK');
  assert.equal(await native(payload,'b'.repeat(64)),'REJECT');
  const corrupt=Buffer.from(payload,'base64url');corrupt[corrupt.length-1]^=1;assert.equal(await native(corrupt.toString('base64url')),'REJECT');
  const version=Buffer.from(payload,'base64url');version[0]=2;assert.equal(await native(version.toString('base64url')),'REJECT');
  const key=Buffer.from(payload,'base64url');key[50]^=1;assert.equal(await native(key.toString('base64url')),'REJECT');
  assert.equal(await native(payload.slice(0,-7)),'REJECT');
  assert.equal(await native(payload+'!'),'REJECT');
  const large=page(pub);await large.callback('A'.repeat(2048));large.click();assert.equal(await native(large.form.children[0].value,state,'A'.repeat(2048)),'OK');
  const max=page(pub);await max.callback('A'.repeat(4095));max.click();assert.equal(await native(max.form.children[0].value,state,'A'.repeat(4095)),'OK');
  const fallback=page(pub);fallback.pollWith(token);for(let i=0;i<200&&fallback.button.disabled;i++)await new Promise(r=>setTimeout(r,10));assert(!fallback.button.disabled);fallback.click();assert.equal(await native(fallback.form.children[0].value),'OK');
  const oversized=page(pub);await oversized.callback('A'.repeat(4096));assert(oversized.button.disabled&&oversized.notice.includes('exceeds'));
  const old=page(pub,undefined,{limit:'2048'});await old.callback('A'.repeat(2049));assert(old.button.disabled&&old.notice.includes('exceeds'));
  for(const url of ['http://8.8.8.8:12345/captcha-return','http://127.0.0.1:12345/captcha-return','https://192.168.1.2:12345/captcha-return','http://192.168.1.2:12345/other','http://192.168.01.2:12345/captcha-return','http://192.168.1.2:12345/captcha-return?q=x'])assert(!page(pub,url).callback);
  assert(!page(pub,undefined,{state:'bad'}).callback);
  assert(!/console\.(log|error)|localStorage|fetch\(|XMLHttpRequest/.test(script));
  console.log('PASS phone page + actual mbedTLS: WebCrypto RSA-OAEP/AES-GCM roundtrip, 2048/4095-byte tokens, visible oversize rejection, state/tag/key/version/truncation rejection, private destination validation');
 }finally{clearTimeout(timeout);child.stdin.end();}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
