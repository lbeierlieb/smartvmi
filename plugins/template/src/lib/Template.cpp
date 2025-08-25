#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.0.0"
#endif

#ifndef BUILD_VERSION
#define BUILD_VERSION "TestBuild"
#endif

#include "Template.h"
#include "Filenames.h"
#include <tclap/CmdLine.h>

// Ensure API compatibility between plugin and VmiCore. When loading a plugin the API version against which it was
// compiled is checked in VmiCore.
VMI_PLUGIN_API_VERSION_INFO

using VmiCore::ActiveProcessInformation;
using VmiCore::ILogger;
using VmiCore::Plugin::IPlugin;
using VmiCore::Plugin::IPluginConfig;
using VmiCore::Plugin::PluginInterface;

namespace Template
{
    Template::Template(VmiCore::Plugin::PluginInterface* pluginInterface,
                       const VmiCore::Plugin::IPluginConfig& config,
                       std::vector<std::string>& args)
        : logger(pluginInterface->newNamedLogger(TEMPLATE_LOGGER_NAME))
    {

        // Create the objects which contain the logic of the plugin
        templateCode = std::make_unique<TemplateCode>(pluginInterface, pluginInterface->getIntrospectionAPI());
    }

    void Template::unload() {
        logger->info("Terminating bpbench", {{"breakpoint hits counted", templateCode->getBreakpointHits()}});
    }
}

// This is the init function. It is linked and called dynamically at runtime by
// VmiCore and is responsible for creating an instance of a plugin.
// All plugins have to inherit from IPlugin.
extern "C" std::unique_ptr<IPlugin> VmiCore::Plugin::vmicore_plugin_init(
    PluginInterface* pluginInterface,
    std::shared_ptr<IPluginConfig> config, // NOLINT(performance-unnecessary-value-param)
    std::vector<std::string> args)
{
    return std::make_unique<Template::Template>(pluginInterface, *config, args);
}
