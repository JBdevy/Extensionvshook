#!/bin/bash
# Equivalente macOS de "build extensão hook.bat". Compatível com Bash 3.2.
set -euo pipefail
export PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/local/bin:$PATH"

finish() {
  local status=$?
  if [ "$status" -ne 0 ]; then
    printf '\nERRO: a operação não foi concluída. Confira a mensagem acima.\n'
    printf 'Commits ou etapas de envio já concluídos foram preservados.\n'
  fi
  if [ -t 0 ]; then read -r -p 'Pressione Enter para fechar...' _ || true; fi
  exit "$status"
}
trap finish EXIT
fail() { printf '\nERRO: %s\n' "$*" >&2; exit 1; }

cd -- "$(dirname -- "$0")"
printf '\nSUBIR VSHookExt + DISPARAR WINDOWS E MACOS\n\n'
printf 'Atualiza a versão no CMakeLists.txt, cria o commit e envia a branch.\n'
printf 'A tag vX.Y.Z dispara a release com Windows x64 e macOS Universal.\n\n'

command -v git >/dev/null 2>&1 || fail 'Git não foi encontrado no PATH.'
command -v perl >/dev/null 2>&1 || fail 'Perl não foi encontrado no PATH.'
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || fail 'Esta pasta não é um repositório Git.'
[ -f CMakeLists.txt ] || fail 'CMakeLists.txt não foi encontrado.'
[ "$(wc -c < CMakeLists.txt)" -lt 1048576 ] || fail 'CMakeLists.txt atingiu 1 MB; confira o arquivo antes de enviar.'
branch=$(git symbolic-ref --quiet --short HEAD) || fail 'Selecione uma branch antes de executar.'
current_version=$(perl -0777 -ne '
  my @versions = /project\(reaper_vshook_loader VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)/g;
  die "Linha de versão ausente ou duplicada em CMakeLists.txt.\n" unless @versions == 1;
  print $versions[0];
' CMakeLists.txt) || fail 'Não foi possível identificar a versão da extensão.'

while :; do
  read -r -p "Versão da VSHookExt [$current_version]: " version || exit 1
  version=${version:-$current_version}
  case "$version" in [vV]*) version=${version:1} ;; esac
  if [[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then break; fi
  printf '\nUse o formato X.Y.Z, por exemplo 1.0.2.\n\n'
done
tag_name="v$version"
default_message="VSHookExt $version"
read -r -p "Mensagem do commit [$default_message]: " commit_message || exit 1
commit_message=${commit_message:-$default_message}

printf '\nConsultando a tag no repositório remoto...\n'
remote_tag=$(git ls-remote --tags --refs origin "refs/tags/$tag_name") || fail 'Não foi possível consultar a tag remota. Confira o login do Git.'
remote_oid=${remote_tag%%$'\t'*}
local_tag_exists=false
if git show-ref --verify --quiet "refs/tags/$tag_name"; then local_tag_exists=true; fi

printf '\nBranch: %s\nVersão: %s\nTag: %s\nCommit: %s\n' "$branch" "$version" "$tag_name" "$commit_message"
printf 'Saídas: reaper_VSHookExt.dll e reaper_VSHookExt.dylib\n'
printf 'Todas as alterações desta pasta serão incluídas no commit.\n'
if [ "$local_tag_exists" = true ] || [ -n "$remote_oid" ]; then
  printf 'A tag %s já existe e será recriada, como no BAT.\n' "$tag_name"
fi
read -r -p 'Continuar com o commit, envio e disparo das builds? [S/N]: ' confirmation || exit 1
case "$confirmation" in
  [sS]|[sS][iI][mM]) ;;
  *) printf '\nOperação cancelada. Nada foi alterado ou enviado.\n'; exit 0 ;;
esac

printf '\nAtualizando a versão no CMakeLists.txt...\n'
perl - "$version" <<'PERL'
use strict;
use warnings;
my $version = shift @ARGV;
my $file = 'CMakeLists.txt';
open my $in, '<:raw', $file or die "Não foi possível ler $file: $!\n";
local $/;
my $raw = <$in>;
close $in or die "Não foi possível fechar $file: $!\n";
die "CMakeLists.txt atingiu 1 MB.\n" if length($raw) >= 1048576;
my $next = $raw;
my $count = ($next =~ s/project\(reaper_vshook_loader VERSION [0-9]+\.[0-9]+\.[0-9]+ LANGUAGES CXX\)/project(reaper_vshook_loader VERSION $version LANGUAGES CXX)/g);
die "Linha de versão ausente ou duplicada.\n" unless $count == 1;
die "CMakeLists.txt atingiu 1 MB.\n" if length($next) >= 1048576;
if ($next ne $raw) {
  open my $out, '>:raw', $file or die "Não foi possível gravar $file: $!\n";
  print {$out} $next or die "Falha ao gravar $file: $!\n";
  close $out or die "Falha ao fechar $file: $!\n";
}
PERL

printf '\nAdicionando arquivos...\n'
git add -A
diff_status=0
git diff --cached --quiet || diff_status=$?
case "$diff_status" in
  0) printf 'Nenhuma alteração para criar commit.\n' ;;
  1) git commit -m "$commit_message" ;;
  *) fail 'Não foi possível verificar as alterações preparadas para o commit.' ;;
esac

printf '\nEnviando branch %s...\n' "$branch"
git -c push.followTags=false push origin "refs/heads/$branch:refs/heads/$branch"

if [ -n "$remote_oid" ]; then
  printf '\nRemovendo tag remota existente %s...\n' "$tag_name"
  # A tag só é removida se ainda apontar para o objeto confirmado acima.
  git -c push.followTags=false push \
    --force-with-lease="refs/tags/$tag_name:$remote_oid" origin ":refs/tags/$tag_name"
fi
if [ "$local_tag_exists" = true ]; then
  git tag -d "$tag_name"
fi
printf '\nCriando tag %s...\n' "$tag_name"
git tag -a "$tag_name" -m "$commit_message"
printf '\nEnviando tag e disparando as builds...\n'
git -c push.followTags=false push origin "refs/tags/$tag_name:refs/tags/$tag_name"

printf '\nPRONTO — tag %s enviada.\n' "$tag_name"
printf 'O GitHub Actions vai compilar Windows x64 e macOS Universal.\n'
printf 'Acompanhe: https://github.com/JBdevy/Extensionvshook/actions\n'
printf 'Releases: https://github.com/JBdevy/Extensionvshook/releases\n'
