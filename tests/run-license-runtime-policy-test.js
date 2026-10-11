const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const root = path.resolve(__dirname, '..');
const source = fs.readFileSync(path.join(root, 'src/vshook_extension.cpp'), 'utf8');
const validator = fs.readFileSync(path.join(root, 'src/license_validator.cpp'), 'utf8');
assert(source.includes('g_nativeLicenseSessionAuthorized.store(true)'));
assert(source.includes('if (!g_nativeLicenseSessionAuthorized.load())'));
assert(!source.includes('licenseNow >= nextLicenseCheck'), 'A live show must not be periodically deauthorized');
assert(!source.includes('É necessário conectar à internet para validar sua licença.'), 'Remove recurring reconnection warnings');
assert(!validator.includes('expiredLicenseResult') && !validator.includes('readClockState'), 'No renewal deadline or helper-owned clock sidecar');

// Compile the real verifier with a freshly generated *test* public key. No
// production token, signing secret or shared activation file is read/modified.
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-offline-activation-'));
try {
  const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', { modulusLength: 2048 });
  const jwk = publicKey.export({ format: 'jwk' });
  const array = (name, bytes) => `constexpr unsigned char ${name}[] = {${Array.from(bytes).join(',')}};`;
  fs.writeFileSync(path.join(temporary, 'license-test-key.h'), [
    '#pragma once', 'namespace vshook_license_key {',
    'constexpr const char* kKeyId = "test-only-offline";',
    array('kRsaExponent', Buffer.from(jwk.e, 'base64url')),
    array('kRsaModulus', Buffer.from(jwk.n, 'base64url')),
    array('kRsaPkcs1Der', publicKey.export({ format: 'der', type: 'pkcs1' })), '}'
  ].join('\n'));
  fs.writeFileSync(path.join(temporary, 'license_validator.cpp'),
    validator.replace('#include "license_public_key.h"', '#include "license-test-key.h"'));
  const issued = 2_000_000_000, day = 86_400, machine = 'LICENSE-TEST-PC', hardware = 'A'.repeat(64);
  const header = { alg: 'RS256', typ: 'VSHOOK-LICENSE', kid: 'test-only-offline' };
  const payload = { v: 1, p: 'VSLIVE', m: machine, f: hardware, iat: issued, nbf: issued-300, exp: issued+15*day, ct: 1 };
  const sign = (body, head = header) => {
    const input = [head, body].map(value => Buffer.from(JSON.stringify(value)).toString('base64url')).join('.');
    return input + '.' + crypto.sign('RSA-SHA256', Buffer.from(input), privateKey).toString('base64url');
  };
  const fixtures = [];
  const add = (name, token, failure, now = issued, localMachine = machine, localHardware = hardware) =>
    fixtures.push([name, token, localMachine, localHardware, now, failure].join('\t'));
  const activation = sign(payload);
  for (const days of [0, 14, 15, 18, 365, 3650]) add(`offline-${days}-days`, activation, 0, issued+days*day);
  const legacy = { ...payload }; delete legacy.ct;
  add('legacy-activation-years-offline', sign(legacy), 0, issued+3650*day);
  add('missing-token', '', 1);
  add('malformed-token', 'invalid.token', 2);
  add('wrong-product', sign({ ...payload, p: 'OTHER' }), 2);
  add('wrong-token-version', sign({ ...payload, v: 2 }), 2);
  add('wrong-key-id', sign(payload, { ...header, kid: 'other' }), 2);
  add('tampered-payload', activation.replace(activation.split('.')[1], Buffer.from(JSON.stringify({ ...payload, m: 'ALTERED' })).toString('base64url')), 3);
  add('wrong-machine', activation, 4, issued, 'OTHER-PC');
  add('missing-machine', activation, 4, issued, '');
  add('wrong-hardware', activation, 5, issued, machine, 'B'.repeat(64));
  add('missing-hardware', activation, 5, issued, machine, '');
  add('future-activation', sign({ ...payload, nbf: issued+day }), 6);
  const malformed = { ...payload }; delete malformed.exp;
  add('missing-renewal-metadata', sign(malformed), 2);
  add('unsupported-clock-metadata', sign({ ...payload, ct: 2 }), 2);
  add('missing-issue-date', sign({ ...payload, iat: 0 }), 2);
  fs.writeFileSync(path.join(temporary, 'fixtures.tsv'), fixtures.join('\n')+'\n');
  const windows = process.platform === 'win32';
  assert(windows || process.platform === 'darwin', 'Signature backend test requires macOS or Windows');
  const executable = path.join(temporary, windows ? 'activation.exe' : 'activation');
  const sources = [path.join(temporary, 'license_validator.cpp'), path.join(__dirname, 'license_local_activation_test.cpp')];
  const include = path.join(root, 'src');
  const args = windows
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/DNOMINMAX', '/I'+include, '/Fe:'+executable,
      ...sources, 'advapi32.lib', 'bcrypt.lib']
    : ['-std=c++17', '-O2', '-Wall', '-Wextra', '-I', include, ...sources,
      '-framework', 'CoreFoundation', '-framework', 'Security', '-o', executable];
  for (const [command, parameters] of [
    [windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, [path.join(temporary, 'fixtures.tsv')]]
  ]) {
    const result = spawnSync(command, parameters, { cwd: temporary, encoding: 'utf8' });
    process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    assert.equal(result.status, 0, 'License policy verification failed');
  }
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
