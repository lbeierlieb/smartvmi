#ifndef VMICORE_PLUGININTERFACE_H
#define VMICORE_PLUGININTERFACE_H

#include "../os/ActiveProcessInformation.h"
#include "../types.h"
#include "../vmi/IBreakpoint.h"
#include "../vmi/IIntrospectionAPI.h"
#include "IPluginConfig.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace VmiCore::Plugin
{
    using processStartCallback_f = void (*)(std::shared_ptr<const ActiveProcessInformation>);
    using processTerminationCallback_f = void (*)(std::shared_ptr<const ActiveProcessInformation>);

    using shutdownCallback_f = void (*)();

    enum class LogLevel
    {
        debug,
        info,
        warning,
        error
    };

    class PluginInterface
    {
      public:
        constexpr static uint8_t API_VERSION = 13;

        virtual ~PluginInterface() = default;

        [[nodiscard]] virtual std::unique_ptr<std::vector<uint8_t>>
        readProcessMemoryRegion(pid_t pid, addr_t address, size_t numberOfBytes) const = 0;

        [[nodiscard]] virtual std::unique_ptr<std::vector<std::shared_ptr<const ActiveProcessInformation>>>
        getRunningProcesses() const = 0;

        virtual void registerProcessStartEvent(processStartCallback_f startCallback) = 0;

        virtual void registerProcessTerminationEvent(processTerminationCallback_f terminationCallback) = 0;

        virtual void registerShutdownEvent(shutdownCallback_f shutdownCallback) = 0;

        [[nodiscard]] virtual std::shared_ptr<IBreakpoint>
        createBreakpoint(uint64_t targetVA,
                         uint64_t processDtb,
                         const std::function<BpResponse(IInterruptEvent&)>& callbackFunction) = 0;

        [[nodiscard]] virtual std::unique_ptr<std::string> getResultsDir() const = 0;

        virtual void writeToFile(const std::string& filename, const std::string& message) const = 0;

        virtual void writeToFile(const std::string& filename, const std::vector<uint8_t>& data) const = 0;

        virtual void logMessage(LogLevel logLevel, const std::string& filename, const std::string& message) const = 0;

        virtual void sendErrorEvent(const std::string_view& message) const = 0;

        virtual void sendInMemDetectionEvent(const std::string_view& message) const = 0;

        [[nodiscard]] virtual std::shared_ptr<IIntrospectionAPI> getIntrospectionAPI() const = 0;

      protected:
        PluginInterface() = default;
    };
}

#endif // VMICORE_PLUGININTERFACE_H