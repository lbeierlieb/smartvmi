#include "InterruptEventSupervisor.h"
#include "Event.h"
#include "InterruptGuard.h"
#include "VmiException.h"
#include <memory>
#include <vmicore/callback.h>
#include <vmicore/filename.h>
#include <vmicore/os/PagingDefinitions.h>

namespace VmiCore
{

    namespace
    {
        InterruptEventSupervisor* interruptEventSupervisor = nullptr;
        constexpr auto loggerName = FILENAME_STEM;
    }

    InterruptEventSupervisor::InterruptEventSupervisor(
        std::shared_ptr<ILibvmiInterface> vmiInterface,
        std::shared_ptr<ISingleStepSupervisor> singleStepSupervisor,
        std::shared_ptr<IActiveProcessesSupervisor> activeProcessesSupervisor,
        std::shared_ptr<IRegisterEventSupervisor> registerEventSupervisor,
        std::shared_ptr<ILogging> loggingLib)
        : vmiInterface(std::move(vmiInterface)),
          singleStepSupervisor(std::move(singleStepSupervisor)),
          activeProcessesSupervisor(std::move(activeProcessesSupervisor)),
          registerEventSupervisor(std::move(registerEventSupervisor)),
          loggingLib(std::move(loggingLib)),
          logger(this->loggingLib->newNamedLogger(loggerName))
    {
        interruptEventSupervisor = this;
    }

    InterruptEventSupervisor::~InterruptEventSupervisor() noexcept
    {
        interruptEventSupervisor = nullptr;
    }

    void InterruptEventSupervisor::initialize()
    {
        SETUP_INTERRUPT_EVENT(event, _defaultInterruptCallback);
        event->interrupt_event.reinject = DONT_REINJECT_INTERRUPT;
        event->interrupt_event.insn_length = 1;
        vmiInterface->registerEvent(*event);
        singleStepSupervisor->initializeSingleStepEvents();
        singleStepCallbackFunction = VMICORE_SETUP_SAFE_MEMBER_CALLBACK(singleStepCallback);
        contextSwitchCallbackFunction = VMICORE_SETUP_SAFE_MEMBER_CALLBACK(contextSwitchCallback);
        registerEventSupervisor->setContextSwitchCallback(contextSwitchCallbackFunction);
    }

    void InterruptEventSupervisor::teardown()
    {
        clearInterruptEventHandling();
        singleStepSupervisor->teardown();
        registerEventSupervisor->teardown();
    }

    std::shared_ptr<IBreakpoint>
    InterruptEventSupervisor::createBreakpoint(uint64_t targetVA,
                                               const ActiveProcessInformation& processInformation,
                                               const std::function<BpResponse(IInterruptEvent&)>& callbackFunction,
                                               bool global)
    {
        auto processDtb = targetVA >= PagingDefinitions::kernelspaceLowerBoundary ? processInformation.processDtb
                                                                                  : processInformation.processUserDtb;
        auto targetPA = vmiInterface->convertVAToPA(targetVA, processDtb);
        auto targetGFN = targetPA >> PagingDefinitions::numberOfPageIndexBits;
        auto breakpoint = std::make_shared<Breakpoint>(
            targetPA,
            [supervisor = weak_from_this()](Breakpoint* bp)
            {
                if (auto supervisorShared = supervisor.lock())
                {
                    supervisorShared->deleteBreakpoint(bp);
                }
            },
            callbackFunction,
            processDtb,
            global);

        pageGuard = createPageGuard(targetVA, processDtb, targetGFN);

        // Register new INT3
        vmiInterface->flushV2PCache(LibvmiInterface::flushAllPTs);
        vmiInterface->flushPageCache();
        this->targetPA = targetPA;
        storeOriginalValue(targetPA);
        enableEvent(targetPA);

        this->breakpoint = breakpoint;
            enableEvent(targetPA);
        return breakpoint;
    }

    void InterruptEventSupervisor::deleteBreakpoint(IBreakpoint* breakpoint)
    {
    }

    void InterruptEventSupervisor::enableEvent(addr_t targetPA)
    {
        vmiInterface->write8PA(targetPA, INT3_BREAKPOINT);
        breakpointStatus = BPStateResponse::Enable;
    }

    void InterruptEventSupervisor::disableEvent(addr_t targetPA)
    {
        vmiInterface->write8PA(targetPA, originalValue);
        breakpointStatus = BPStateResponse::Disable;
    }

    event_response_t InterruptEventSupervisor::_defaultInterruptCallback([[maybe_unused]] vmi_instance_t vmi,
                                                                         vmi_event_t* event)
    {
        event->interrupt_event.reinject = DONT_REINJECT_INTERRUPT;
        return interruptEventSupervisor->interruptCallback(
            event->vcpu_id);
    }

    event_response_t InterruptEventSupervisor::interruptCallback(
        uint32_t vcpuId)
    {
        bool deactivateInterrupt = false;

        vmiInterface->flushV2PCache(LibvmiInterface::flushAllPTs);
        vmiInterface->flushPageCache();

        try
        {
            auto eventResponse = breakpoint->callback(interruptEvent);
            if (eventResponse == BpResponse::Deactivate)
            {
                deactivateInterrupt = true;
            }
        }
        catch (const std::exception& e)
        {
            GlobalControl::logger()->error("Interrupt callback failed",
                                           {{"logger", loggerName}, {"exception", e.what()}});
            GlobalControl::eventStream()->sendErrorEvent(e.what());
        }

        disableEvent(targetPA);

        if (!deactivateInterrupt)
        {
          singleStepSupervisor->setSingleStepCallback(vcpuId, singleStepCallbackFunction, targetPA);
        }

        return VMI_EVENT_RESPONSE_NONE;
    }

    void InterruptEventSupervisor::singleStepCallback(vmi_event_t* singleStepEvent)
    {
        enableEvent(reinterpret_cast<addr_t>(singleStepEvent->data));
    }

    void InterruptEventSupervisor::contextSwitchCallback(vmi_event_t* registerEvent)
    {
        // do nothing on context switch - we just want to measure the overhead of the CR3 write events and not some
        // logic that runs here
    }

    std::shared_ptr<InterruptGuard>
    InterruptEventSupervisor::createPageGuard(uint64_t targetVA, uint64_t processDtb, uint64_t targetGFN)
    {
        auto interruptGuard =
            std::make_shared<InterruptGuard>(vmiInterface, loggingLib, targetVA, targetGFN, processDtb);
        interruptGuard->initialize();

        return interruptGuard;
    }

    void InterruptEventSupervisor::storeOriginalValue(addr_t targetPA)
    {
        auto originalValue = vmiInterface->read8PA(targetPA);
        GlobalControl::logger()->debug(
            "Save original value",
            {{"targetPA", fmt::format("{:#x}", targetPA)}, {"originalValue", fmt::format("{:#x}", originalValue)}});
        if (originalValue == INT3_BREAKPOINT)
        {
            throw VmiException(
                fmt::format("{}: Breakpoint originalValue @ {:#x} is already an INT3 breakpoint.", __func__, targetPA));
        }
        this->originalValue = originalValue;
    }

    void InterruptEventSupervisor::clearInterruptEventHandling()
    {
    }

    void
    InterruptEventSupervisor::eraseBreakpointAtAddress(std::vector<std::shared_ptr<Breakpoint>>& breakpointsAtAddress,
                                                       const IBreakpoint* breakpoint)
    {
    }

    void InterruptEventSupervisor::removeInterrupt(addr_t targetPA)
    {
    }

    BPStateResponse
    InterruptEventSupervisor::getNewBreakpointState(reg_t newDtb,
                                                    const std::vector<std::shared_ptr<Breakpoint>>& breakpoints)
    {
        using enum BPStateResponse;
                return Enable;
    }

    void InterruptEventSupervisor::updateBreakpointState(BPStateResponse state, uint64_t targetPA) const
    {
    }
}
