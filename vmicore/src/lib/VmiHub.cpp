#include "VmiHub.h"
#include "GlobalControl.h"
#include "os/linux/ActiveProcessesSupervisor.h"
#include "os/linux/SystemEventSupervisor.h"
#include "os/windows/ActiveProcessesSupervisor.h"
#include "os/windows/SystemEventSupervisor.h"
#include "plugins/PluginException.h"
#include <csignal>
#include <memory>
#include <utility>
#include <vmicore/filename.h>

namespace VmiCore
{
    namespace
    {
        int exitCode = 0;
        constexpr auto loggerName = FILENAME_STEM;
    }

    VmiHub::VmiHub(std::shared_ptr<IConfigParser> configInterface,
                   std::shared_ptr<ILibvmiInterface> vmiInterface,
                   std::shared_ptr<ILogging> loggingLib,
                   std::shared_ptr<IEventStream> eventStream,
                   std::shared_ptr<IFileTransport> pluginTransport,
                   std::shared_ptr<ISingleStepSupervisor> singleStepSupervisor,
                   std::shared_ptr<IRegisterEventSupervisor> contextSwitchHandler)
        : configInterface(std::move(configInterface)),
          vmiInterface(std::move(vmiInterface)),
          loggingLib(std::move(loggingLib)),
          logger(this->loggingLib->newNamedLogger(loggerName)),
          eventStream(std::move(eventStream)),
          pluginTransport(std::move(pluginTransport)),
          singleStepSupervisor(std::move(singleStepSupervisor)),
          contextSwitchHandler(std::move(contextSwitchHandler))
    {
    }

    void VmiHub::waitForEvents() const
    {
        // TODO: only set postRunPluginAction to true after sample process is started
        GlobalControl::postRunPluginAction = true;
#ifdef TRACE_MODE
        auto loopStart = std::chrono::steady_clock::now();
#endif
        while (!GlobalControl::endVmi)
        {
            try
            {
#ifdef TRACE_MODE
                auto callStart = std::chrono::steady_clock::now();
                vmiInterface->waitForEvent();
                auto currentTime = std::chrono::steady_clock::now();
                auto callDuration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - callStart);
                auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - loopStart);
                logger->debug(
                    "Event loop call",
                    {{"durationMilliseconds", callDuration.count()}, {"totalElapsedTimeSeconds", elapsedTime.count()}});
#else
                vmiInterface->eventsListen(500);
#endif
            }
            catch (const std::exception& e)
            {
                logger->error("Error while waiting for events", {{"exception", e.what()}});
                eventStream->sendErrorEvent(e.what());
                logger->info("Trying to get the VM state");

                exitCode = 1;
                GlobalControl::endVmi = true;
            }
        }
    }

    void logReceivedSignal(int signal)
    {
        if (signal > 0)
        {
            switch (signal)
            {
                case SIGINT:
                {
                    GlobalControl::logger()->info("externalInterruptHandler: SIGINT received",
                                                  {{"logger", loggerName}});
                    break;
                }
                case SIGTERM:
                {
                    GlobalControl::logger()->info("externalInterruptHandler: SIGTERM received",
                                                  {{"logger", loggerName}});
                    break;
                }
                default:
                {
                    GlobalControl::logger()->error("Called for unhandled signal. This should never occur",
                                                   {{"logger", loggerName}});
                }
            }
        }
    }

    void externalInterruptHandler(int signal)
    {
        exitCode = 128 + signal;
        logReceivedSignal(signal);
        GlobalControl::endVmi = true;
    }

    void setupSignalHandling()
    {
        struct sigaction sigactionStruct{};
        sigactionStruct.sa_handler = &externalInterruptHandler;
        auto status = sigaction(SIGINT, &sigactionStruct, nullptr);
        if (status != 0)
        {
            throw std::runtime_error("Unable to register SIGINT action handler.");
        }
        status = sigaction(SIGTERM, &sigactionStruct, nullptr);
        if (status != 0)
        {
            throw std::runtime_error("Unable to register SIGTERM action handler.");
        }
    }

    uint VmiHub::run(const std::map<std::string, std::vector<std::string>, std::less<>>& pluginArgs)
    {
        vmiInterface->initializeVmi();
        std::shared_ptr<IActiveProcessesSupervisor> activeProcessesSupervisor;
        std::shared_ptr<IInterruptEventSupervisor> interruptEventSupervisor;
        switch (vmiInterface->getOsType())
        {
            case OperatingSystem::LINUX:
            {
                activeProcessesSupervisor =
                    std::make_shared<Linux::ActiveProcessesSupervisor>(vmiInterface, loggingLib, eventStream);
                interruptEventSupervisor = std::make_shared<InterruptEventSupervisor>(
                    vmiInterface, singleStepSupervisor, activeProcessesSupervisor, contextSwitchHandler, loggingLib);

                pluginSystem = std::make_shared<PluginSystem>(configInterface,
                                                              vmiInterface,
                                                              activeProcessesSupervisor,
                                                              interruptEventSupervisor,
                                                              pluginTransport,
                                                              loggingLib,
                                                              eventStream);
                systemEventSupervisor = std::make_shared<Linux::SystemEventSupervisor>(vmiInterface,
                                                                                       pluginSystem,
                                                                                       activeProcessesSupervisor,
                                                                                       configInterface,
                                                                                       interruptEventSupervisor,
                                                                                       loggingLib,
                                                                                       eventStream);

                break;
            }
            case OperatingSystem::WINDOWS:
            {
                auto kernelObjectExtractor = std::make_shared<Windows::KernelAccess>(vmiInterface);
                activeProcessesSupervisor = std::make_shared<Windows::ActiveProcessesSupervisor>(
                    vmiInterface, kernelObjectExtractor, loggingLib, eventStream);
                interruptEventSupervisor = std::make_shared<InterruptEventSupervisor>(
                    vmiInterface, singleStepSupervisor, activeProcessesSupervisor, contextSwitchHandler, loggingLib);
                pluginSystem = std::make_shared<PluginSystem>(configInterface,
                                                              vmiInterface,
                                                              activeProcessesSupervisor,
                                                              interruptEventSupervisor,
                                                              pluginTransport,
                                                              loggingLib,
                                                              eventStream);
                systemEventSupervisor = std::make_shared<Windows::SystemEventSupervisor>(vmiInterface,
                                                                                         pluginSystem,
                                                                                         activeProcessesSupervisor,
                                                                                         configInterface,
                                                                                         interruptEventSupervisor,
                                                                                         loggingLib,
                                                                                         eventStream);
                break;
            }
            default:
            {
                throw std::runtime_error("Unknown operating system.");
            }
        }

        vmiInterface->pauseVm();
        systemEventSupervisor->initialize();
        try
        {
            pluginSystem->initializePlugins(pluginArgs);
            vmiInterface->resumeVm();

            eventStream->sendReadyEvent();

            setupSignalHandling();
            waitForEvents();

            vmiInterface->pauseVm();
        }
        catch (const PluginException& e)
        {
            logger->error("Failed to initialize plugin", {{"Plugin", e.plugin()}, {"Exception", e.what()}});
            eventStream->sendErrorEvent(e.what());
        }

        if (GlobalControl::postRunPluginAction)
        {
            pluginSystem->unloadPlugins();
        }
        systemEventSupervisor->teardown();
        vmiInterface->resumeVm();

        return exitCode;
    }
}
