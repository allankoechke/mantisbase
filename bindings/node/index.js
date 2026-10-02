const path = require('path');

// `prebuild-install` (see the "install" script in package.json) downloads a
// prebuilt binary into build/Release/ when one exists for this platform, and
// falls back to a cmake-js compile otherwise. Either way the addon ends up in
// one of the locations below, so loading is just a matter of finding it.
const CANDIDATES = [
    path.join(__dirname, 'build', 'Release', 'mantisbase.node'),
    path.join(__dirname, 'build', 'Debug', 'mantisbase.node'),
    path.join(__dirname, 'prebuilds', `${process.platform}-${process.arch}`, 'mantisbase.node'),
];

let binding;
const errors = [];

for (const candidate of CANDIDATES) {
    try {
        binding = require(candidate);
        break;
    } catch (err) {
        errors.push(`  ${candidate}: ${err.message}`);
    }
}

if (!binding) {
    throw new Error(
        'Failed to load the mantisbase native addon. Tried:\n' +
        errors.join('\n') +
        '\n\nBuild it from source with: npx cmake-js build'
    );
}

module.exports = binding;
