'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const kStringMaxLength = process.binding('buffer').kStringMaxLength;

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'toobig.txt');
const stream = fs.createWriteStream(file, {
  flags: 'a'
});

const size = kStringMaxLength / 200;
const a = Buffer.alloc(
  common.isChakraEngine ? Math.trunc(size) : size).fill('a');

for (var i = 0; i < 201; i++) {
  stream.write(a);
}

stream.end();
stream.on('finish', common.mustCall(function() {
  // make sure that the toString does not throw an error
  fs.readFile(file, 'utf8', common.mustCall(function(err, buf) {
    if (common.isChakraEngine) { // chakra does not fail at this limit
      return;
    }
    assert.ok(err instanceof Error);
    assert.strictEqual('"toString()" failed', err.message);
  }));
}));

function destroy() {
  try {
    fs.unlinkSync(file);
  } catch (err) {
    // it may not exist
  }
}

process.on('exit', destroy);

process.on('SIGINT', function() {
  destroy();
  process.exit();
});

// To make sure we don't leave a very large file
// on test machines in the event this test fails.
process.on('uncaughtException', function(err) {
  destroy();
  throw err;
});
