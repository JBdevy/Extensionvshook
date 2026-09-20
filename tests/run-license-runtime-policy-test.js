const assert = require('assert')
const fs = require('fs')
const path = require('path')

const source = fs.readFileSync(path.resolve(__dirname, '..', 'src', 'vshook_extension.cpp'), 'utf8')
const validator = fs.readFileSync(path.resolve(__dirname, '..', 'src', 'license_validator.cpp'), 'utf8')

assert(source.includes('g_nativeLicenseSessionAuthorized.store(true)'),
  'Sessão não é travada depois da validação inicial')
assert(source.includes('if (!g_nativeLicenseSessionAuthorized.load())'),
  'Timer não respeita a licença aceita na abertura')
assert(!source.includes('licenseNow >= nextLicenseCheck'),
  'Extensão ainda revalida e pode fechar durante o show')
assert(source.includes('É necessário conectar à internet para validar sua licença.'),
  'Mensagem de conexão obrigatória ausente')
assert(validator.includes('warningStartedAt = now'),
  'Carência não começa na primeira observação do vencimento')
assert(validator.includes('kOfflineWarningSeconds = 3 * 24 * 60 * 60'),
  'Carência obrigatória não tem três dias')
assert(validator.includes('warningCount < 3 &&'),
  'Os três avisos não são contados separadamente')
assert(validator.includes('now >= lastWarningAt + (24 * 60 * 60)'),
  'O número do aviso pode avançar mais de uma vez no mesmo dia')
assert(source.includes('"AVISO " + std::to_string(warningNumber) + " DE 3'),
  'Aviso não mostra a etapa diária')

console.log('LICENSE_RUNTIME_POLICY_OK: aviso garantido e sessão do show preservada.')
