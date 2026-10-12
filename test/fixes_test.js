#!/usr/bin/env node
'use strict';
const f = require(__dirname + '/../build/Release/flowjs_native');

function test(label, src) {
  try {
    const ast = f.parse(src);
    console.log('✓', label);
  } catch(e) {
    console.log('✗', label, ':', e.message.substring(0, 70));
  }
}

test('labelled', 'foo: while(true) { break foo; }');
test('async fn', 'async function foo() { return 1; }');
test('destructuring params', 'function f([a], {b}) { return a + b; }');
test('destructuring var', 'var [a, b] = [1, 2];');
test('keyword keys', 'var x = { if: 42, true: false };');
test('computed keys', 'var x = { ["a" + "b"]: 1 };');
test('generator method', 'var x = { *method() { yield 1; } };');
test('spread in obj', 'var x = { ...a, b: 1 };');
test('method shorthand', 'var x = { foo() { return 1; } };');
test('shorthand props', 'var x = { a, b, c };');
test('for of', 'for (let x of [1,2,3]) { console.log(x); }');
test('ternary', 'var x = a ? b : c;');