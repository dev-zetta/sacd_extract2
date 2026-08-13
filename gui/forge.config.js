const fs = require('node:fs');
const path = require('node:path');

const { FusesPlugin } = require('@electron-forge/plugin-fuses');
const { FuseV1Options, FuseVersion } = require('@electron/fuses');

function extractorResource() {
  const executableName = process.platform === 'win32'
    ? 'sacd_extract.exe'
    : 'sacd_extract';
  const configuredPath = process.env.SACD_EXTRACT_BINARY;
  const extractorPath = path.resolve(
    configuredPath || path.join(__dirname, executableName)
  );

  if (!fs.existsSync(extractorPath)) {
    throw new Error(
      `Extractor binary not found at ${extractorPath}. ` +
      'Set SACD_EXTRACT_BINARY before packaging.'
    );
  }

  return extractorPath;
}

module.exports = {
  packagerConfig: {
    asar: true,
    extraResource: [extractorResource()],
    ignore: [
      /^\/\.vscode(?:\/|$)/,
      /^\/test(?:\/|$)/,
    ],
  },
  rebuildConfig: {},
  makers: [],
  plugins: [
    new FusesPlugin({
      version: FuseVersion.V1,
      [FuseV1Options.RunAsNode]: false,
      [FuseV1Options.EnableCookieEncryption]: true,
      [FuseV1Options.EnableNodeOptionsEnvironmentVariable]: false,
      [FuseV1Options.EnableNodeCliInspectArguments]: false,
      [FuseV1Options.EnableEmbeddedAsarIntegrityValidation]: true,
      [FuseV1Options.OnlyLoadAppFromAsar]: true,
    }),
  ],
};
