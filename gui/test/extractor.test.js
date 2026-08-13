'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');
const test = require('node:test');

const {
  buildExtractorArguments,
  classifyExtractionResult,
  extractorExecutableName,
  getExtractorCandidates,
  parseExtractorVersion,
} = require('../src/extractor');

test('uses platform-native extractor names', () => {
  assert.equal(
    extractorExecutableName('linux'),
    'sacd_extract'
  );
  assert.equal(
    extractorExecutableName('win32'),
    'sacd_extract.exe'
  );
});

test('prefers an explicit extractor path', () => {
  const candidates = getExtractorCandidates({
    platform: 'linux',
    resourcesPath: '/application/resources',
    appDirectory: '/repository/gui/src',
    environment: {
      SACD_EXTRACT_PATH: '/custom/sacd_extract',
      PATH: '/usr/local/bin:/usr/bin',
    },
  });

  assert.equal(
    candidates[0],
    path.resolve('/custom/sacd_extract')
  );
  assert.ok(
    candidates.includes(
      path.join('/application/resources', 'sacd_extract')
    )
  );
  assert.ok(
    candidates.includes('/repository/build/linux/sacd_extract')
  );
  assert.ok(
    candidates.includes('/usr/bin/sacd_extract')
  );
});

test('builds current long-form extractor arguments', () => {
  assert.deepEqual(
    buildExtractorArguments({
      source: '/music/Album.iso',
      output: '/music/output',
      format: 'dsf',
      area: 'both',
      decodeDst: true,
      exportCuesheet: true,
    }),
    [
      '--input', '/music/Album.iso',
      '--dsf',
      '--track-output-dir', '/music/output',
      '--stereo', '--multichannel',
      '--convert-dst',
      '--cue',
    ]
  );
});

test('builds multichannel DSDIFF arguments', () => {
  assert.deepEqual(
    buildExtractorArguments({
      source: 'Album.iso',
      output: 'output',
      format: 'dff',
      area: 'multichannel',
      decodeDst: false,
      exportCuesheet: false,
    }),
    [
      '--input', 'Album.iso',
      '--dsdiff',
      '--track-output-dir', 'output',
      '--multichannel',
    ]
  );
});

test('parses the adopted extractor version output', () => {
  assert.equal(
    parseExtractorVersion(
      'sacd_extract client 0.4.4\nProgram terminates!'
    ),
    '0.4.4'
  );
  assert.equal(parseExtractorVersion('invalid'), null);
});

test('maps the extractor exit-status contract to GUI states', () => {
  assert.equal(
    classifyExtractionResult(0).state,
    'completed'
  );
  assert.equal(
    classifyExtractionResult(1).state,
    'partial'
  );
  assert.equal(
    classifyExtractionResult(2).state,
    'error'
  );
  assert.equal(
    classifyExtractionResult(130).state,
    'cancelled'
  );
  assert.equal(
    classifyExtractionResult(null, true).state,
    'cancelled'
  );
});
