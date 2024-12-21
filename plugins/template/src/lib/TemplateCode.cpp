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
        // Register required events
        logger->error("Registering process start tracker;");
        // pluginInterface->registerProcessStartEvent(VMICORE_SETUP_MEMBER_CALLBACK(doStuffWithProcessStart));
        auto processes = pluginInterface->getRunningProcesses();
        logger->error("", {{"num processes", processes->size()}});
        for (auto process : *processes) {
            if (process->name.compare("BPBENCH.EXE") == 0) {
                uint64_t *mutprocess = (uint64_t *) &process->processUserDtb;
                *mutprocess = process->processDtb;
                logger->error("found", {{"name", process->name}, {"pid", process->pid}, {"cr3", process->processDtb}, {"user cr3", process->processUserDtb}});
                // auto content = this->lowLevelIntrospectionApi->read8VA(0x2091a4f0000, process->processUserDtb);
                // logger->error("found", {{"name", process->name}, {"content", (int)content}});
                this->pluginInterface->createBreakpoint(0x2091a4f0fff, *process, VMICORE_SETUP_MEMBER_CALLBACK(emptyCallback));
            }
        }
    }

    // As an example when a new process is started this callback is called. Check the PluginInterface.h for additional
    // available events.
    void TemplateCode::doStuffWithProcessStart(std::shared_ptr<const ActiveProcessInformation> processInformation)
    {
        // if (processInformation->name.compare("TRIGGER.EXE") == 0) {
        //     auto processes = pluginInterface->getRunningProcesses();
        //     for (auto process : *processes) {
        //         if (process->name.compare("BPBENCH.EXE") != 0) {
        //             continue;
        //         }

        //         auto addr = 0x2091a4f0001;
        //         logger->error("found, inserting breakpoint at ", {{"addr", addr}});
        //         pluginInterface->createBreakpoint(addr, *process, VMICORE_SETUP_MEMBER_CALLBACK(emptyCallback));
        //         break;
        //     }
        // }
        // if (processInformation->name.compare("BPBENCH.EXE") == 0) {
        //     bpBenchProcessInfo = processInformation;
        //     bpBenchPid = processInformation->pid;
        //     logger->error("BPBENCH.EXE observed, storing activeProcessInformation");
        // } else if (processInformation->name.compare("TRIGGER.EXE") == 0) {
        //     //auto content = lowLevelIntrospectionApi->read64VA(0x2091a4f0000, bpBenchProcessInfo->processUserDtb);
        //     //auto content = lowLevelIntrospectionApi->convertVAToPA(0x2091a4f0000, bpBenchProcessInfo->processUserDtb);
        //     // auto lookupDtb = lowLevelIntrospectionApi->convertPidToDtb(bpBenchProcessInfo->pid);
        //     logger->error("TRIGGER bserved, would create bp now ");
        //     //logger->error("Process start triggered.",
        //     // {{"processDtb stored", bpBenchProcessInfo->processUserDtb},
        //     //  {"processDtb lookup", lookupDtb},
        //     //  {"pid", bpBenchPid},
        //     //  {"read", content}});

        //     //pluginInterface->createBreakpoint(0x2091a4f0000, *bpBenchProcessInfo, VMICORE_SETUP_MEMBER_CALLBACK(emptyCallback));
        //     pluginInterface->createBreakpoint(0x7ff72de616f3, *bpBenchProcessInfo, VMICORE_SETUP_MEMBER_CALLBACK(emptyCallback));
        // }
    }

    VmiCore::BpResponse TemplateCode::emptyCallback(VmiCore::IInterruptEvent&) {
        breakpointHits++;
        return VmiCore::BpResponse::Continue;
    }

    int TemplateCode::getBreakpointHits() {
        return breakpointHits;
    }
}
