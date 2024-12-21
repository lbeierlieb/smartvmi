#ifndef TEMPLATE_TEMPLATECODE_H
#define TEMPLATE_TEMPLATECODE_H

#include <vmicore/os/ActiveProcessInformation.h>
#include <vmicore/plugins/PluginInterface.h>

namespace Template
{
    class TemplateCode
    {
      public:
        TemplateCode(VmiCore::Plugin::PluginInterface* pluginInterface,
                     std::shared_ptr<VmiCore::IIntrospectionAPI> lowLevelIntrospectionApi);

        void doStuffWithProcessStart(std::shared_ptr<const VmiCore::ActiveProcessInformation> processInformation);
        VmiCore::BpResponse emptyCallback(VmiCore::IInterruptEvent&);
        int getBreakpointHits();

      private:
        VmiCore::Plugin::PluginInterface* pluginInterface;
        std::unique_ptr<VmiCore::ILogger> logger;
        std::shared_ptr<VmiCore::IIntrospectionAPI> lowLevelIntrospectionApi;
        std::shared_ptr<const VmiCore::ActiveProcessInformation> bpBenchProcessInfo;
        pid_t bpBenchPid;
        int breakpointHits;
    };
}

#endif // TEMPLATE_TEMPLATECODE_H
