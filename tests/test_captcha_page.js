const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync(require('path').join(__dirname,'../app/assets/captcha.html'),'utf8');
const script=html.match(/<script>\s*([\s\S]*?)<\/script>/)[1];
function context(hash){let options,data,returned,cleared=false,notice='';
 const c={URLSearchParams,location:{hash,pathname:'/discordnx2-captcha.html',replace:u=>returned=u},history:{replaceState:()=>cleared=true},document:{getElementById:()=>({set textContent(v){notice=v;}})},hcaptcha:{render:(id,o)=>{options=o;return 7},setData:(id,d)=>{assert.equal(id,7);data=d}}};
 vm.createContext(c);vm.runInContext(script,c);vm.runInContext('ready()',c);
 return {get options(){return options},get data(){return data},get returned(){return returned},get cleared(){return cleared},get notice(){return notice}};
}
const state='a'.repeat(64),c=context('#state='+state+'&sitekey=test-site&rqdata=test%2Bdata');
assert(c.cleared);assert.equal(c.options.sitekey,'test-site');assert.equal(c.data.rqdata,'test+data');
c.options.callback('synthetic/solution+');assert.equal(c.returned,'https://discordnx2.invalid/captcha-return#state='+state+'&token=synthetic%2Fsolution%2B');
c.options.callback('second');assert(c.returned.endsWith('synthetic%2Fsolution%2B'));
assert.equal(context('#state=bad&sitekey=test').options,undefined);
assert(!/console\.(log|error)|localStorage|fetch\(/.test(script));
console.log('PASS CAPTCHA page: fragment-only challenge, enterprise data, one-shot callback, invalid request, no storage/logging');
