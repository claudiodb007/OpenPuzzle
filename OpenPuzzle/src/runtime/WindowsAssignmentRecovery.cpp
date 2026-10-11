#include "openpuzzle/runtime/WindowsAssignmentRecovery.hpp"
#include "openpuzzle/runtime/WindowsRunSession.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/SolutionExporter.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/runtime/WindowsEngineRange.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <set>
#include <algorithm>
#include <cctype>
#include <sstream>
namespace openpuzzle {
int recoverWindowsAssignment(const std::string& slot,const std::string& requestedServer,std::ostream& out){
 using Control=ClientRuntimeControl;using Store=client::ClientStateStore;
 try {
  if(Store::executionSlot()!=slot)throw std::runtime_error("Recovery slot differs from the selected execution slot");
  if(Control::runtimeIdentityStatus(slot)!=Control::RuntimeIdentityStatus::Inactive)throw std::runtime_error("Runtime is active or its identity is unavailable");
  const auto pending=WindowsRunSession::pendingClaimPath(slot);
  windowsFile::Lock receiptLock(pending.wstring()+L".lock");
  if(!receiptLock.locked())throw std::runtime_error("Claim receipt is unavailable or occupied");
  bool missing=false;const auto text=windowsFile::readPrivate(pending,missing,16384);
  if(!text)throw std::runtime_error("No readable claim receipt; local data preserved");
  boost::property_tree::ptree receipt;std::istringstream input(*text);boost::property_tree::read_json(input,receipt);
  std::set<std::string> fields;for(const auto& item:receipt)if(item.first.empty()||!fields.insert(item.first).second||!item.second.empty())throw std::runtime_error("Ambiguous receipt");
  if(receipt.get<int>("version")!=1||receipt.get<std::string>("status","")=="requesting")throw std::runtime_error("Claim response was uncertain; no new claim or automatic cancellation is allowed");
  const auto state=Store::load(slot);if(!state)throw std::runtime_error("Assignment state is absent or unreadable; receipt cannot prove server acknowledgement");
  const auto server=receipt.get<std::string>("server");
  if(!client::HttpRequestPolicy::endpoint(server)||(!requestedServer.empty()&&server!=requestedServer))throw std::runtime_error("Recovery server differs from the persisted claim");
  auto engine=state->engine;std::transform(engine.begin(),engine.end(),engine.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
  if(receipt.get<std::string>("engine")!=engine)throw std::runtime_error("Engine differs from the preserved receipt");
  const auto normalized=[](const std::string& value){return WindowsEngineRange::normalized(value);};
  if(receipt.get<std::string>("assignment_id")!=state->assignmentId||receipt.get<std::string>("client_id")!=state->clientId||
     receipt.get<int>("puzzle")!=state->puzzle||receipt.get<int>("range_id")!=state->rangeId||
     normalized(receipt.get<std::string>("start"))!=normalized(state->start)||normalized(receipt.get<std::string>("end"))!=normalized(state->end)||
     receipt.get<std::string>("target")!=state->target||receipt.get<std::string>("backend")!=state->backend||
     receipt.get<std::string>("search_mode")!="linear"||!windowsExecutionRecord(*state)||
     !RunAssignmentPlan::safeAssignmentId(state->assignmentId)||pathFromUtf8(state->workspace)!=localPaths::dataDirectory()/"assignments"/pathFromUtf8(state->assignmentId))
    throw std::runtime_error("Recovery state differs from the protected claim receipt");
  bool identityMissing=false;const auto identity=windowsFile::readPrivate(localPaths::configDirectory()/"client.id",identityMissing,64);
  if(!identity||identity->substr(0,36)!=state->clientId)throw std::runtime_error("Stored client identity differs from the assignment");
  if(!Control::acquire())throw std::runtime_error("Recovery cannot own the execution slot");
  struct Release {~Release(){(void)Control::release();}} release;
  const auto observed=client::ExecutionSyncService().inspect(slot);
  if(!observed.hasState||observed.running||observed.identityUnavailable)throw std::runtime_error("Engine is running or its generation is unavailable; no upload attempted");
  if(observed.solutionFound){
   const auto exported=client::SolutionExporter::exportSolution(observed.state,observed.solutionPath);
   if(!exported.success)throw std::runtime_error("Solution export failed; original result preserved: "+exported.error);
   client::HttpRangeClient http(server);
   if(!http.reportSolution(state->assignmentId,state->clientId))throw std::runtime_error("Solution notification failed; retry preserves the local result");
   out<<"Solution exported privately: "<<exported.walletPath<<"\nReview notification acknowledged; state and receipt retained\nNo private key uploaded; no assignment requested\n";return 0;
  }
  const auto result=client::ExecutionSyncService().tick(server,slot);
  if(result.identityUnavailable||!result.stateRemoved||(!result.completionUploaded&&result.completionStatus!=client::AssignmentUploadStatus::AssignmentRejected))
    throw std::runtime_error("Recovery is not acknowledged; state and receipt preserved: "+result.completionError);
  bool absent=false;const auto current=windowsFile::readPrivate(pending,absent,16384);
  if(!current||*current!=*text)throw std::runtime_error("Receipt changed during recovery; preserved");
  std::error_code error;if(!std::filesystem::remove(pending,error)||error)throw std::runtime_error("Acknowledged receipt cleanup failed; review preserved receipt");
  out<<"Recovered assignment: "<<state->assignmentId<<"\nServer acknowledgement confirmed; execution state and receipt cleared\nNo engine restarted; no new assignment requested\n";return 0;
 }catch(const std::exception& error){out<<"Windows recovery refused: "<<error.what()<<"\nExisting local data preserved\n";return 1;}
}
}
