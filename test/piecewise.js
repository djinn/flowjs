#!/usr/bin/env node
'use strict';
const flow = require(__dirname + '/../build/Release/flowjs_native');

function test(label, src) {
  try {
    const ast = flow.parse(src);
    console.log(`✓ ${label}: OK`);
    return JSON.parse(ast);
  } catch(e) {
    console.error(`✗ ${label}: ${e.message}`);
    return null;
  }
}

test('var decl', 'var x = 1;');
test('let decl', 'let x = 1;');
test('const decl', 'const x = 1;');
test('fn decl', 'function f() { return 1; }');
test('arrow', 'const f = (x) => x + 1;');
test('template', 'const m = `hello`;');
test('optional', 'const v = obj?.prop;');
test('class', 'class A { constructor() {} }');
test('ternary', 'const t = x > 0 ? 1 : 2;');
test('object', 'const o = { a: 1, b: 2 };');