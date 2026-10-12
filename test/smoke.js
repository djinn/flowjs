#!/usr/bin/env node
// Quick smoke test for flowjs
'use strict';

const flow = require('../build/Release/flowjs_native');

console.log('flowjs version:', flow.version);

// Test 1: simple parse
const src = `let x = 42;
const y = "hello";
var z = x + y;
function add(a, b) {
  return a + b;
}
// test optional chaining
const val = obj?.prop?.nested;
// template literal
const msg = \`Hello, \${name}! Welcome.\`;
// arrow function
const square = (n) => n * n;
`;

try {
  const result = flow.parse(src);
  console.log('\n=== PARSE RESULT (AST JSON) ===');
  console.log(result.substring(0, 200) + '...');
  console.log('\nParse OK!');
} catch (e) {
  console.error('PARSE ERROR:', e.message);
  process.exit(1);
}

// Test 2: transformFile
const fs = require('fs');
const testFile = '/tmp/flowjs-test-input.js';
fs.writeFileSync(testFile, src, 'utf-8');

try {
  const result = flow.transformFile(testFile);
  console.log('\n=== TRANSFORM FILE RESULT ===');
  console.log(result.substring(0, 300) + '...');
  console.log('\nTransformFile OK!');
} catch (e) {
  console.error('TRANSFORM FILE ERROR:', e.message);
  process.exit(1);
}

console.log('\n✓ All smoke tests passed!');