#pragma once
#include "GpuControl.hpp"
#include <memory>
namespace openpuzzle {
// Explicit C++ injection seam for driver fixtures, never selected by a CLI flag,
// DLL path or environment variable. Production resolves only the system driver.
struct NvmlControlPci {char legacy[16];unsigned domain,bus,device,deviceId,subsystemId;char busId[32];};
struct NvmlControlApi {
 int(*count)(unsigned*)=nullptr;int(*handle)(unsigned,void**)=nullptr;
 int(*name)(void*,char*,unsigned)=nullptr;int(*uuid)(void*,char*,unsigned)=nullptr;int(*pci)(void*,NvmlControlPci*)=nullptr;
 int(*limit)(void*,unsigned*)=nullptr;int(*defaults)(void*,unsigned*)=nullptr;int(*numFans)(void*,unsigned*)=nullptr;
 int(*constraints)(void*,unsigned*,unsigned*)=nullptr;int(*fanRange)(void*,unsigned*,unsigned*)=nullptr;
 int(*policy)(void*,unsigned,unsigned*)=nullptr;int(*target)(void*,unsigned,unsigned*)=nullptr;int(*temperature)(void*,unsigned,unsigned*)=nullptr;
 int(*setPower)(void*,unsigned)=nullptr;int(*setFan)(void*,unsigned,unsigned)=nullptr;int(*autoFan)(void*,unsigned)=nullptr;
};
std::unique_ptr<GpuControlBackend> makeNvmlControlFixture(const NvmlControlApi&);
}
