/* flowjs Node.js library entry point */
'use strict';

const path = require('path');

// Locate the native module
const buildDir = __dirname ? __dirname + '/../build/Release' : './build/Release';
let native;
try {
  native = require(buildDir + '/flowjs_native');
} catch (e) {
  try {
    native = require('flowjs_native');
  } catch (e2) {
    throw new Error('Cannot load flowjs native module. Run `npm run build` first.');
  }
}

function parse(source, options) {
  if (typeof source !== 'string') throw new TypeError('source must be a string');
  const filename = (options && options.filename) ? options.filename : undefined;
  const result = native.parse(source, filename);
  if (typeof result === 'string') {
    return JSON.parse(result);
  }
  return result;
}

function transformFile(filePath, options = {}) {
  const result = native.transformFile(filePath);
  if (typeof result === 'string') {
    return JSON.parse(result);
  }
  return result;
}

module.exports = {
  version: native.version,
  parse,
  transformFile,
  // Expose native for advanced usage
  native,
};