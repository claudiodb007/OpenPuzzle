#include "openpuzzle/workers/WorkerAgentFactory.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

int main() {

    ExecutionResource resource;

    resource.name="RTX4070";

    resource.engine="BitCrack";

    resource.backend="CUDA";

    resource.device=0;

    resource.capability.engine="BitCrack";

    resource.capability.backend="CUDA";

    resource.capability.device=0;

    auto worker=
        WorkerAgentFactory::create(resource);

    assert(worker.info().engine=="BitCrack");

    assert(worker.info().backend=="CUDA");

    assert(worker.info().capabilities.size()==1);

    assert(worker.bestCapability(
        "BitCrack",
        "CUDA"));

    std::cout
        <<"WorkerAgentFactoryTests passed\n";
}
