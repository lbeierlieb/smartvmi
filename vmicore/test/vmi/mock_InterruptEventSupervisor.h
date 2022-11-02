#ifndef VMICORE_MOCK_INTERRUPTEVENTSUPERVISOR_H
#define VMICORE_MOCK_INTERRUPTEVENTSUPERVISOR_H

#include "../../src/vmi/InterruptEventSupervisor.h"
#include <gmock/gmock.h>

namespace VmiCore
{
    class MockInterruptEventSupervisor : public IInterruptEventSupervisor
    {
      public:
        MOCK_METHOD(void, initialize, (), (override));

        MOCK_METHOD(void, teardown, (), (override));

        MOCK_METHOD(std::shared_ptr<IBreakpoint>,
                    createBreakpoint,
                    (uint64_t, uint64_t, const std::function<BpResponse(IInterruptEvent&)>&),
                    (override));

        MOCK_METHOD(void, deleteBreakpoint, (IBreakpoint*), (override));
    };
}

#endif // VMICORE_MOCK_INTERRUPTEVENTSUPERVISOR_H