#pragma once

#include <filesystem>
#include <memory>
#include <set>
#include <stdexcept>
#include <vmicore/plugins/IPluginConfig.h>
#include <vmicore/plugins/PluginInterface.h>

namespace InMemoryScanner
{
    class ConfigException : public std::runtime_error
    {
      public:
        explicit ConfigException(const std::string& Message) : std::runtime_error(Message.c_str()) {};
    };

    class IConfig
    {
      public:
        virtual ~IConfig() = default;

        virtual void parseConfiguration(const VmiCore::Plugin::IPluginConfig& config) = 0;

        [[nodiscard]] virtual std::filesystem::path getSignatureFile() const = 0;

        [[nodiscard]] virtual std::filesystem::path getOutputPath() const = 0;

        [[nodiscard]] virtual int getScanTimeout() const = 0;

        [[nodiscard]] virtual bool isProcessIgnored(const std::string& processName) const = 0;

        [[nodiscard]] virtual bool isScanAllRegionsActivated() const = 0;

        [[nodiscard]] virtual bool isDumpingMemoryActivated() const = 0;

        virtual void overrideDumpMemoryFlag(bool value) = 0;

      protected:
        IConfig() = default;
    };

    class Config : public IConfig
    {
      public:
        explicit Config(const VmiCore::Plugin::PluginInterface* pluginInterface);

        ~Config() override = default;

        void parseConfiguration(const VmiCore::Plugin::IPluginConfig& config) override;

        [[nodiscard]] std::filesystem::path getSignatureFile() const override;

        [[nodiscard]] std::filesystem::path getOutputPath() const override;

        [[nodiscard]] int getScanTimeout() const override;

        [[nodiscard]] bool isProcessIgnored(const std::string& processName) const override;

        [[nodiscard]] bool isScanAllRegionsActivated() const override;

        [[nodiscard]] bool isDumpingMemoryActivated() const override;

        void overrideDumpMemoryFlag(bool value) override;

      private:
        std::unique_ptr<VmiCore::ILogger> logger;
        std::filesystem::path outputPath;
        std::filesystem::path signatureFile;
        std::set<std::string> ignoredProcesses;
        bool dumpMemory{};
        bool scanAllRegions{};
        int scanTimeout;
    };
}
