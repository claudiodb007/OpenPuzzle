#include "openpuzzle/runtime/RunSession.hpp"
#include <cassert>
#include <iostream>
using openpuzzle::RunSession;
int main() {
 const RunSession session;
 assert(session.run({"run","71","--engine","vanity"})==1);
 assert(session.run({"claim","71","--engine","VANITY-PREVIEW"})==1);
 assert(session.run({"run","71","--engine","vanity","--backend","cuda","--vanity-session","start"})==2);
 assert(session.run({"run","20","--engine","vanity","--backend","opencl","--vanity-session","start"})==2);
 assert(session.run({"run","20","--engine","vanity","--backend","cuda","--vanity-session","start","--store","/tmp/missing"})==2);
 assert(session.run({"run","20","--engine","vanity","--backend","cuda","--vanity-session","runs","--store","/tmp/missing","--server","https://example.invalid"})==2);
 assert(session.run({"run","20","--engine","vanity","--backend","cuda","--vanity-native-local","--vanity-native-local"})==2);
 assert(session.run({"run","20","--engine","vanity","--engine","bitcrack","--backend","cuda","--vanity-session","runs","--store","/tmp/missing"})==2);
 assert(session.run({"run","20","--vanity-session","runs","--store","/tmp/missing"})==2);
 assert(session.run({"run","71","--engine","vanity","--receipt-protocol","openpuzzle_vanity_receipts_v1"})==1);
 assert(session.run({"run","20","--receipt-protocol","openpuzzle_vanity_receipts_v1"})==1);
 assert(session.run({"run","20","--engine","vanity","--backend","cuda","--vanity-session","runs","--store","/tmp/missing","--receipt-protocol","openpuzzle_vanity_receipts_v1"})==2);
 assert(session.run({"run","20","--engine","vanity","--backend","cuda","--vanity-native-local","--receipt-protocol","openpuzzle_vanity_receipts_v1","--receipt-protocol","openpuzzle_vanity_receipts_v1"})==2);
 std::cout<<"RunSessionRouteTests passed: production gate, strict local selection, duplicate/extra options and no ordinary fallback.\n";
}
