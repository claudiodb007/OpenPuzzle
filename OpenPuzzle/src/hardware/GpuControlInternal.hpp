#pragma once
#include "openpuzzle/hardware/GpuControl.hpp"
#include <memory>
namespace openpuzzle {
std::unique_ptr<GpuControlBackend> makeNvmlControl();
std::unique_ptr<GpuControlBackend> makeAmdControl();
std::string controlPci(unsigned domain,unsigned bus,unsigned device,unsigned function);
}
