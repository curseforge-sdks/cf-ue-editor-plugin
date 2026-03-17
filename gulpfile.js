const { execSync } = require('child_process');
const { src, dest, series } = require('gulp');
const zip = require('gulp-zip');
const del = require('del');

const kPluginFolderName = 'cfeditor';
const kPluginDescriptorName = 'cfeditor.uplugin';
const kDistributionName = 'cfeditor';
const kNpmCommand = process.platform === 'win32' ? 'npm.cmd' : 'npm';

// -----------------------------------------------------------------------------
const clean = () => {
  return del([
    'dist',
    'output',
  ]);
};

// -----------------------------------------------------------------------------
const getBumpType = () => {
  let bumpType = 'minor';

  for (let i = 0; i < process.argv.length; ++i) {
    const arg = process.argv[i];
    if (arg.substring(0, '-bump:'.length) === '-bump:') {
      bumpType = arg.substring('-bump:'.length);

      // npm can split `-bump:patch` into separate args. Handle that shape too.
      if ((!bumpType) ||
        (bumpType.length === 0 && process.argv.length >= i + 1)) {
        bumpType = process.argv[i + 1];
      }
    }
  }

  return bumpType;
};

const runCommand = (command) => {
  console.log(command);
  execSync(command, { stdio: 'inherit' });
};

const bumpPackage = (done) => {
  try {
    const bumpType = getBumpType();
    runCommand(`${kNpmCommand} version ${bumpType} --no-git-tag-version`);
    done();
  } catch (error) {
    done(error);
  }
};

// -----------------------------------------------------------------------------
const bumpUPlugin = (done) => {
  try {
    runCommand(`"${process.execPath}" ./build-scripts/bump-versions.js`);
    done();
  } catch (error) {
    done(error);
  }
};

// -----------------------------------------------------------------------------
const copyPluginFiles = () => {
  return src([
    './Content/**',
    './docs/**',
    './Resources/**',
    './Source/**',
    `./${kPluginDescriptorName}`,
    './LICENSE',
    './README.md',
  ], { base: '.' })
    .pipe(dest(`./output/${kPluginFolderName}`));
};

// -----------------------------------------------------------------------------
const packageDistribution = () => {
  const pkg = require('./package.json');

  return src([
    './output/**/*'
  ])
    .pipe(zip(`${kDistributionName}-${pkg.version}.zip`))
    .pipe(dest('./dist/'));
};

// -----------------------------------------------------------------------------
exports.dist = series(
  clean,
  bumpPackage,
  bumpUPlugin,
  copyPluginFiles,
  packageDistribution,
);
