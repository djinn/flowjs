#!/usr/bin/env node
/**
 * flowjs-vs-Babel Benchmark
 * Final version — proper timing with process.hrtime().
 */
'use strict';
const fs = require('fs'), path = require('path'), os = require('os');
const flowjs = require(__dirname + '/../build/Release/flowjs_native');
const babel  = require('@babel/parser');

const WARMUP = 5;
const FIXTURE_DIR = '/tmp/flowjs-bench-fixtures';

function hrtime() { const h=process.hrtime(); return h[0]*1e9+h[1]; } // nanoseconds

function gen(id) {
  return [
    `var a${id}=1;let b${id}=2;const c${id}=3;`,
    `function fn${id}(x,y){return x+y;}`,
    `const g${id}=n=>n*n;`,
    `const o${id}={x:1,y:2};`,
    `if(b${id}>0){a${id}=b${id};}`,
    `for(let i${id}=0;i${id}<10;i${id}++){if(i${id}>5)break;}`,
    `const v${id}=b${id}>0?1:2;`,
    `const[f${id},...r${id}]=[1,2,3];`,
    `switch(b${id}){case 1:break;case 2:break;default:break;}`,
    `const w${id}=a${id}?.b${id}?.c${id};`,
    `try{fn${id}(1,2);}catch(e${id}){b${id};}finally{c${id};}`,
  ].join('');
}

function genBench(kb) {
  let r = '';
  for(let i=1; Buffer.from(r,'utf-8').length<kb*1024; i++) r += gen(i);
  return r;
}

fs.mkdirSync(FIXTURE_DIR, {recursive:true});
const sizes = {tiny:0.3, small:2, medium:20, large:100, xlarge:500};
const iters = {tiny:500, small:200, medium:100, large:50, xlarge:20};

console.log('=== Generating ===');
for(const[n,kb] of Object.entries(sizes)){
  const c=genBench(kb);
  fs.writeFileSync(path.join(FIXTURE_DIR,n+'.js'),c,'utf-8');
  console.log(` ${n.padEnd(7)} ${Math.round(c.length/1024)} kB`);
}

function bench(fn, source, it) {
  for(let i=0;i<WARMUP;i++) try{fn(source);}catch{}
  const t=[];
  for(let i=0;i<it;i++){
    const t0=hrtime();
    try{fn(source);t.push((hrtime()-t0)/1e6);}catch(e){}
  }
  if(!t.length) return {error:`all ${it} failed`};
  t.sort((a,b)=>a-b);
  const n=t.length, sum=t.reduce((a,b)=>a+b,0), mean=sum/n;
  const med=n%2?t[Math.floor(n/2)]:(t[n/2-1]+t[n/2])/2;
  const p95=t[Math.floor(n*0.95)], min=t[0], max=t[n-1];
  const stddev=Math.sqrt(t.reduce((a,x)=>a+(x-mean)**2,0)/n);
  const tp=mean>0?Math.round((source.length/1024)/(mean/1000)):0;
  return {mean,median:med,min,max,stddev,p95,n,throughput:tp};
}

const check=gen(1);
let fok=1,bok=1;
try{flowjs.parse(check);}catch(e){console.log(' flowjs snippet fails:',e.message.substring(0,60));fok=0;}
try{babel.parse(check,{});}catch(e){console.log(' babel snippet fails:',e.message.substring(0,60));bok=0;}
if(!fok||!bok){console.log('ABORT');process.exit(1);}
console.log(' Both parsers accept snippet\n');

console.log('=== Benchmarks ===\n');
const results={};
for(const[n,kb] of Object.entries(sizes)){
  const c=fs.readFileSync(path.join(FIXTURE_DIR,n+'.js'),'utf-8');
  const it=iters[n], skb=Math.round(c.length/1024);
  console.log(`${n.padEnd(7)} ${skb} kB  (${it} iter)`);
  console.log('-'.repeat(58));
  const fb=bench(s=>flowjs.parse(s),c,it);
  const bb=bench(s=>babel.parse(s,{}),c,it);
  results[n]={skb,flow:fb,babel:bb};
  if(fb.error)console.log(' flowjs: '+fb.error);
  else if(bb.error)console.log(' babel: '+bb.error);
  else{
    const sp=bb.mean/fb.mean;
    console.log(` flowjs: ${fb.mean.toFixed(3).padStart(10)} ms  (±${fb.stddev.toFixed(3)})  [${fb.min.toFixed(3)}–${fb.max.toFixed(3)}]`);
    console.log(` babel:  ${bb.mean.toFixed(3).padStart(10)} ms  (±${bb.stddev.toFixed(3)})  [${bb.min.toFixed(3)}–${bb.max.toFixed(3)}]`);
    console.log(` speedup: ${sp.toFixed(1)}×  (${fb.throughput} vs ${bb.throughput} kB/s)`);
  }
  console.log();
}

console.log('='.repeat(68));
console.log('SUMMARY: flowjs vs @babel/parser');
console.log('='.repeat(68));
const hdr=`${'Fixture'.padEnd(8)} ${'Size'.padStart(5)}  ${'flowjs(ms)'.padStart(11)}  ${'babel(ms)'.padStart(11)}  ${'Speed'.padStart(6)}  ${'TP kB/s'.padStart(8)}`;
console.log(hdr); console.log('-'.repeat(68));
let ft=0,bt=0;
for(const[n,d] of Object.entries(results)){
  const f=d.flow,b=d.babel;
  if(f.error||b.error)continue;
  const sp=b.mean/f.mean; ft+=f.mean; bt+=b.mean;
  console.log(`${n.padEnd(7)} ${d.skb.toString().padStart(4)} kB  ${f.mean.toFixed(3).padStart(10)}  ${b.mean.toFixed(3).padStart(10)}  ${sp.toFixed(1).padStart(5)}×  ${f.throughput.toString().padStart(6)} kB/s`);
}
console.log('-'.repeat(68));
console.log(`${'TOTAL'.padEnd(7)} ${''.padStart(4)}    ${ft.toFixed(3).padStart(10)}  ${bt.toFixed(3).padStart(10)}  ${(bt/ft).toFixed(1).padStart(5)}×`);
console.log('='.repeat(68));

console.log(`\nSystem: Node ${process.version}, ${os.type} ${os.arch||''}`);
try{console.log(` CPU: ${os.cpus().length} cores`);}catch{}
console.log(` Date: ${new Date().toISOString()}`);
console.log('\nDone');