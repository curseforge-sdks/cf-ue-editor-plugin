/**
 * This script bumps the plugin descriptor version, based on the version set in
 * package.json.
 */
const fs = require('fs');
const packageJson = require('../package.json');

const kUPluginJsonFilename = 'cfeditor.uplugin';

// -----------------------------------------------------------------------------
const main = async () => {
  console.log(`Bumping version for ${kUPluginJsonFilename} ...`);
  await updateVersion(kUPluginJsonFilename);
};

// -----------------------------------------------------------------------------
const updateVersion = async (uePluginJsonFilename) => {
  const settings = loadSettings(uePluginJsonFilename);

  settings.Version = parseInt(packageJson.version.split('.')[0], 10);
  settings.VersionName = packageJson.version;

  saveUpdatedProjectFile(settings, uePluginJsonFilename);
};

// -----------------------------------------------------------------------------
const loadSettings = (uePluginJsonFilename) => {
  const raw = fs.readFileSync(uePluginJsonFilename, 'utf8');
  const result = JSON.parse(raw);
  return result;
};

// -----------------------------------------------------------------------------
const saveUpdatedProjectFile = (projectSettings, projectFile) => {
  const raw = JSON.stringify(projectSettings, null, 2);
  fs.writeFileSync(projectFile, raw, 'utf8');
};

main();
