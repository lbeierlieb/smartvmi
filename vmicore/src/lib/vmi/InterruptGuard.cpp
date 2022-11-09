#include "InterruptGuard.h"
#include "../GlobalControl.h"
#include "../os/PagingDefinitions.h"
#include "VmiException.h"
#include <fmt/core.h>
#include <memory>
#include <utility>
#include <vmicore/filename.h>

namespace VmiCore
{
    namespace
    {
        constexpr auto loggerName = FILENAME_STEM;
    }

    InterruptGuard::InterruptGuard(std::shared_ptr<ILibvmiInterface> vmiInterface,
                                   const std::shared_ptr<ILogging>& logging,
                                   uint64_t targetVA,
                                   uint64_t targetGFN,
                                   uint64_t systemCr3)
        : vmiInterface(std::move(vmiInterface)),
          logger(logging->newNamedLogger(loggerName)),
          targetVA(targetVA),
          targetGFN(targetGFN),
          shadowPage(PagingDefinitions::pageSizeInBytes + 16),
          systemCr3(systemCr3)
    {
    }

    void InterruptGuard::initialize()
    {
        // setting simple read events is unsupported by EPT
        SETUP_MEM_EVENT(&guardEvent, targetGFN, VMI_MEMACCESS_RW, &InterruptGuard::_guardCallback, false);
        guardEvent.data = this;

        // This will never change so we initialize this here once
        emulateReadData.dont_free = true;
        emulateReadData.size = 16;

        auto pageBaseVA = targetVA & PagingDefinitions::stripPageOffsetMask;
        // we need a small buffer of data from the subsequent page because memory reads may be overlapping
        if (!vmiInterface->readXVA(pageBaseVA, systemCr3, shadowPage))
        {
            throw VmiException(fmt::format(
                "{}: Unable to create Interrupt @ {:#x} in system with cr3 {:#x}", __func__, pageBaseVA, systemCr3));
        }
        enableEvent();
        logger->debug("Interrupt guard: Register RW event on gfn",
                      {logfield::create("targetGFN",
                                        fmt::format("{:#x}", targetGFN >> PagingDefinitions::numberOfPageIndexBits))});
    }

    void InterruptGuard::teardown()
    {
        disableEvent();
    }

    void InterruptGuard::enableEvent()
    {
        vmiInterface->registerEvent(guardEvent);
    }

    void InterruptGuard::disableEvent()
    {
        vmiInterface->clearEvent(guardEvent, true);
    }

    event_response_t InterruptGuard::_guardCallback(__attribute__((unused)) vmi_instance_t vmiInstance,
                                                    vmi_event_t* event)
    {
        auto eventResponse = VMI_EVENT_RESPONSE_NONE;
        try
        {
            eventResponse = reinterpret_cast<InterruptGuard*>(event->data)->guardCallback(event);
        }
        catch (const std::exception& e)
        {
            GlobalControl::endVmi = true;
            GlobalControl::logger()->error(
                "Unexpected exception",
                {logfield::create("logger", loggerName), logfield::create("exception", e.what())});
            GlobalControl::eventStream()->sendErrorEvent(e.what());
        }
        return eventResponse;
    }

    event_response_t InterruptGuard::guardCallback(vmi_event_t* event)
    {
        auto eventPA = (event->mem_event.gfn << PagingDefinitions::numberOfPageIndexBits) + event->mem_event.offset;
        if (!interruptGuardHit)
        {
            logger->warning("Interrupt guard hit, check if patch guard is active");
            interruptGuardHit = true;
        }
        logger->debug("Interrupt guard hit", {logfield::create("eventPA", fmt::format("{:#x}", eventPA))});
        event->emul_read = &emulateReadData;
        // we are allowed to provide more data than actually needed but empirically no more than 16 bytes are read at a
        // time
        for (int i = 0; i < 16; ++i)
        {
            event->emul_read->data[i] = shadowPage.at(event->mem_event.offset + i);
        }
        return VMI_EVENT_RESPONSE_SET_EMUL_READ_DATA;
    }
}