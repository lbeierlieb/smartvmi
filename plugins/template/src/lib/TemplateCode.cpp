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
        pluginInterface->registerProcessStartEvent(VMICORE_SETUP_MEMBER_CALLBACK(doStuffWithProcessStart));
    }

    // As an example when a new process is started this callback is called. Check the PluginInterface.h for additional
    // available events.
    void TemplateCode::doStuffWithProcessStart(std::shared_ptr<const ActiveProcessInformation> processInformation)
    {
        if (processInformation->name.compare("TRIGGER.EXE") == 0) {
            auto processes = pluginInterface->getRunningProcesses();
            for (auto process : *processes) {
                if (process->name.compare("BPBENCH.EXE") != 0) {
                    continue;
                }

                pluginInterface->createBreakpoint(0x7ff72de616f3, *process, VMICORE_SETUP_MEMBER_CALLBACK(emptyCallback));
                break;
            }
        }
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
        logger->error("hello");
        return VmiCore::BpResponse::Continue;
    }
}
