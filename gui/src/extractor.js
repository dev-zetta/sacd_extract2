'use strict';

const fs = require('node:fs');
const path = require('node:path');

function extractorExecutableName(platform = process.platform) {
  return platform === 'win32'
    ? 'sacd_extract.exe'
    : 'sacd_extract';
}

function getExtractorCandidates({
  platform = process.platform,
  resourcesPath = process.resourcesPath,
  appDirectory = __dirname,
  environment = process.env,
} = {}) {
  const executableName =
    extractorExecutableName(platform);
  const guiRoot = path.resolve(appDirectory, '..');
  const repositoryRoot = path.resolve(guiRoot, '..');
  const candidates = [];

  if (environment.SACD_EXTRACT_PATH) {
    candidates.push(
      path.resolve(environment.SACD_EXTRACT_PATH)
    );
  }

  if (resourcesPath) {
    candidates.push(
      path.join(resourcesPath, executableName)
    );
  }

  candidates.push(
    path.join(guiRoot, executableName),
    path.join(
      repositoryRoot,
      'build',
      platform === 'win32' ? 'windows' : 'linux',
      executableName
    ),
    path.join(
      repositoryRoot,
      'build',
      'release',
      executableName
    ),
    path.join(
      repositoryRoot,
      'build',
      'sacd_extract',
      executableName
    )
  );

  for (const directory of
    String(environment.PATH || '').split(path.delimiter)) {
    if (directory) {
      candidates.push(
        path.join(directory, executableName)
      );
    }
  }

  return [...new Set(candidates)];
}

function findExtractor(options = {}) {
  const candidates = getExtractorCandidates(options);
  const extractorPath = candidates.find((candidate) => {
    try {
      return fs.statSync(candidate).isFile();
    } catch {
      return false;
    }
  });

  if (!extractorPath) {
    throw new Error(
      'sacd_extract was not found.\n\n' +
      'Set SACD_EXTRACT_PATH or place the extractor in one of these locations:\n' +
      candidates.join('\n')
    );
  }

  return extractorPath;
}

function parseExtractorVersion(output) {
  const match = String(output || '').match(
    /sacd_extract\s+client\s+([^\s\r\n]+)/i
  );

  return match?.[1] || null;
}

function classifyExtractionResult(exitCode, cancellationRequested = false) {
  if (cancellationRequested || exitCode === 130) {
    return {
      state: 'cancelled',
      message: 'Extraction was interrupted.',
    };
  }

  if (exitCode === 0) {
    return {
      state: 'completed',
      message: 'Extraction completed successfully.',
    };
  }

  if (exitCode === 1) {
    return {
      state: 'partial',
      message:
        'Extraction completed with partial or abandoned outputs.',
    };
  }

  return {
    state: 'error',
    message: `Extraction failed with exit code ${exitCode}.`,
  };
}

function buildExtractorArguments(options) {
  const args = [
    '--input',
    options.source,
  ];

  if (options.format === 'dff') {
    args.push('--dsdiff');
  } else {
    args.push('--dsf');
  }

  args.push(
    '--track-output-dir',
    options.output
  );

  if (options.area === 'multichannel') {
    args.push('--multichannel');
  } else if (options.area === 'both') {
    args.push('--stereo', '--multichannel');
  } else {
    args.push('--stereo');
  }

  if (options.decodeDst) {
    args.push('--convert-dst');
  }

  if (options.exportCuesheet) {
    args.push('--cue');
  }

  return args;
}

module.exports = {
  buildExtractorArguments,
  classifyExtractionResult,
  extractorExecutableName,
  findExtractor,
  getExtractorCandidates,
  parseExtractorVersion,
};
