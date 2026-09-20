#include "../src/license_validator.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main()
{
  constexpr int64_t day = 24 * 60 * 60;
  constexpr int64_t firstObserved = 2'000'000'000;

  const auto justObserved = vshook_license::evaluateOfflineGrace(
    firstObserved, firstObserved, 1, firstObserved, true);
  assert(justObserved.allowed);
  assert(justObserved.warning);
  assert(justObserved.deadline == firstObserved + 3 * day);

  const auto repeatedFirstDayOpening =
    vshook_license::evaluateOfflineGrace(
      firstObserved + 60, firstObserved,
      1, firstObserved, false);
  assert(repeatedFirstDayOpening.allowed);
  assert(repeatedFirstDayOpening.warning);

  const auto nearlyFinished = vshook_license::evaluateOfflineGrace(
    firstObserved + 3 * day - 1, firstObserved,
    3, firstObserved + 2 * day, false);
  assert(nearlyFinished.allowed);
  assert(nearlyFinished.warning);

  const auto finished = vshook_license::evaluateOfflineGrace(
    firstObserved + 3 * day, firstObserved,
    3, firstObserved + 2 * day, false);
  assert(!finished.allowed);
  assert(!finished.warning);

  // Mesmo que o token tenha vencido ha muito tempo, a primeira observacao
  // inicia os tres dias agora; nunca salta direto para o bloqueio.
  const int64_t firstOpenAfterLongOffline = firstObserved + 30 * day;
  const auto lateFirstOpen = vshook_license::evaluateOfflineGrace(
    firstOpenAfterLongOffline, firstObserved,
    2, firstOpenAfterLongOffline, true);
  assert(lateFirstOpen.allowed);
  assert(lateFirstOpen.warning);

  // Cada dia repete o mesmo numero; o proximo aviso so e contado depois de
  // 24 horas. Depois do terceiro, ainda existe um dia completo antes de uma
  // abertura futura poder bloquear.
  const auto secondWarningAfterFiveDays =
    vshook_license::evaluateOfflineGrace(
      firstObserved + 5 * day, firstObserved,
      2, firstObserved + 5 * day, true);
  assert(secondWarningAfterFiveDays.allowed);
  const auto thirdWarning = vshook_license::evaluateOfflineGrace(
    firstObserved + 6 * day, firstObserved,
    3, firstObserved + 6 * day, true);
  assert(thirdWarning.allowed);
  assert(thirdWarning.deadline == firstObserved + 7 * day);

  std::cout << "LICENSE_OFFLINE_GRACE_OK\n";
  return 0;
}
