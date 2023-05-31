#include "Config.h"
#include "Common.h"
#include "Filenames.h"
#include <algorithm>

using VmiCore::Plugin::IPluginConfig;
using VmiCore::Plugin::PluginInterface;

namespace InMemoryScanner
{
    constexpr uint64_t defaultMaxScanSize = 52428800; // 50MB

    Config::Config(const PluginInterface* pluginInterface)
        : logger(pluginInterface->newNamedLogger(INMEMORY_LOGGER_NAME))
    {
        logger->bind({{VmiCore::WRITE_TO_FILE_TAG, LOG_FILENAME}});
    }

    void Config::parseConfiguration(const IPluginConfig& config)
    {
        auto& rootNode = config.rootNode();
        signatureFile = rootNode["signature_file"].as<std::string>();
        outputPath = rootNode["output_path"].as<std::string>();
        dumpMemory = rootNode["dump_memory"].as<bool>(false);
        scanAllRegions = rootNode["scan_all_regions"].as<bool>(false);
        maximumScanSize = rootNode["maximum_scan_size"].as<uint64_t>(defaultMaxScanSize);

        auto ignoredProcessesVec =
            rootNode["ignored_processes"].as<std::vector<std::string>>(std::vector<std::string>());
        std::copy(ignoredProcessesVec.begin(),
                  ignoredProcessesVec.end(),
                  std::inserter(ignoredProcesses, ignoredProcesses.end()));
        for (auto& element : ignoredProcesses)
        {
            logger->info("Got ignored process", {{"Name", element}});
        }
    }

    std::filesystem::path Config::getSignatureFile() const
    {
        return signatureFile;
    }

    std::filesystem::path Config::getOutputPath() const
    {
        return outputPath;
    }

    bool Config::isProcessIgnored(const std::string& processName) const
    {
        return ignoredProcesses.find(processName) != ignoredProcesses.end();
    }

    bool Config::isScanAllRegionsActivated() const
    {
        return scanAllRegions;
    }

    bool Config::isDumpingMemoryActivated() const
    {
        return dumpMemory;
    }

    uint64_t Config::getMaximumScanSize() const
    {
        return maximumScanSize;
    }

    void Config::overrideDumpMemoryFlag(bool value)
    {
        dumpMemory = value;
    }
}
