#include "openpuzzle/client/ExecutionSyncService.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace openpuzzle::client;
int main() {
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("opv-cancel-"+std::to_string(getpid()));
  fs::create_directory(root);fs::permissions(root,fs::perms::owner_all);
  ClientExecutionState state;state.engine="Vanity";state.workspace=root.string();
  state.assignmentId="11111111-1111-4111-8111-111111111111";
  state.clientId="22222222-2222-4222-8222-222222222222";
  const auto marker=root/"cancel-requested.txt";
  assert(!ExecutionSyncService::cancellationRequested(state));
  assert(ExecutionSyncService::requestCancellation(state));
  assert(ExecutionSyncService::cancellationRequested(state));
  assert(ExecutionSyncService::requestCancellation(state));
  auto different=state;different.assignmentId="33333333-3333-4333-8333-333333333333";
  assert(!ExecutionSyncService::requestCancellation(different));
  assert(ExecutionSyncService::cancellationRequested(state));
  fs::permissions(marker,fs::perms::owner_read|fs::perms::owner_write|fs::perms::others_read);
  assert(!ExecutionSyncService::requestCancellation(state));
  fs::remove(marker);const auto untouched=root/"untouched";
  {std::ofstream file(untouched);file<<"unchanged";}
  fs::create_symlink(untouched,marker);
  assert(!ExecutionSyncService::requestCancellation(state));
  {std::ifstream file(untouched);std::string value;file>>value;assert(value=="unchanged");}
  fs::remove(marker);fs::create_directory(marker);
  assert(!ExecutionSyncService::requestCancellation(state));
  auto other=state;other.engine="BitCrack";
  assert(!ExecutionSyncService::cancellationRequested(other));
  assert(ExecutionSyncService::requestCancellation(other));
  fs::remove_all(root);
  std::cout<<"Vanity cancellation intent: binding, retry and unsafe records passed\n";
}
