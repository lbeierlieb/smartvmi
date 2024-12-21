#include "TemplateCode.h"
#include "Filenames.h"
#include <utility>
#include <vmicore/callback.h>
#include <vmicore/vmi/BpResponse.h>

namespace Template
{
    using VmiCore::ActiveProcessInformation;
    using VmiCore::Plugin::PluginInterface;

    TemplateCode::TemplateCode(VmiCore::Plugin::PluginInterface* pluginInterface,
                               std::shared_ptr<VmiCore::IIntrospectionAPI> lowLevelIntrospectionApi)
        : pluginInterface(pluginInterface),
          logger(this->pluginInterface->newNamedLogger(TEMPLATE_LOGGER_NAME)),
          lowLevelIntrospectionApi(std::move(lowLevelIntrospectionApi))
    {
        // find bpbench process and insert breakpoint
        auto breakpointAddress = 0x2091a4f0fff;
        auto processes = pluginInterface->getRunningProcesses();
        logger->error("", {{"num processes", processes->size()}});
        for (auto process : *processes) {
            if (process->name.compare("BPBENCH.EXE") == 0) {
                // the userDtb has an invalid value, replacing it with kernelDtb as a band-aid
                uint64_t *mutprocess = (uint64_t *) &process->processUserDtb;
                *mutprocess = process->processDtb;
                logger->info("found bpbench process", {{"name", process->name}, {"pid", process->pid}, {"cr3", process->processDtb}});
                this->pluginInterface->createBreakpoint(breakpointAddress, *process, VMICORE_SETUP_MEMBER_CALLBACK(emptyCallback));
                logger->info("Breakpoint in bpbench created", {{"breakpointAddress", breakpointAddress}});
            }
        }
    }

    VmiCore::BpResponse TemplateCode::emptyCallback(VmiCore::IInterruptEvent&) {
        breakpointHits++;
        return VmiCore::BpResponse::Continue;
    }

    int TemplateCode::getBreakpointHits() {
        return breakpointHits;
    }
}
