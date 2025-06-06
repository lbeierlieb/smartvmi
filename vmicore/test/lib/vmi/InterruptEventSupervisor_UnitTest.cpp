#include "../io/mock_EventStream.h"
#include "../io/mock_Logging.h"
#include "../os/windows/mock_ActiveProcessesSupervisor.h"
#include "mock_LibvmiInterface.h"
#include "mock_SingleStepSupervisor.h"
#include <GlobalControl.h>
#include <gtest/gtest.h>
#include <plugins/PluginSystem.h>
#include <vmicore/os/PagingDefinitions.h>
#include <vmicore/vmi/IBreakpoint.h>
#include <vmicore/vmi/VmiException.h>
#include <vmicore_test/io/mock_Logger.h>

using testing::_;
using testing::AnyNumber;
using testing::NiceMock;
using testing::Ref;
using testing::Return;
using testing::SaveArg;

namespace VmiCore
{
    using breakpointCallbackFunction_t = std::function<BpResponse(IInterruptEvent&)>;
    using singleStepCallbackFunction_t = std::function<void(vmi_event_t*)>;

    namespace
    {
        constexpr uint8_t REINJECT_INTERRUPT = 1;
        constexpr uint8_t INT3_BREAKPOINT = 0xCC;
        constexpr vmi_instance* vmiInstanceStub = nullptr;
        constexpr uint64_t testKernelVA1 = 0x4321 * PagingDefinitions::pageSizeInBytes +
                                           PagingDefinitions::kernelspaceLowerBoundary,
                           testKernelVA2 = 0x5432 * PagingDefinitions::pageSizeInBytes +
                                           PagingDefinitions::kernelspaceLowerBoundary,
                           testUserVA1 = 0x8765 * PagingDefinitions::pageSizeInBytes,
                           testUserVA2 = 0x9876 * PagingDefinitions::pageSizeInBytes;
        ;
        constexpr uint64_t testPA1 = 0x1234 * PagingDefinitions::pageSizeInBytes,
                           testPA2 = 0x5678 * PagingDefinitions::pageSizeInBytes;
        constexpr uint64_t testSystemDtb = 0xaaa00000;
        constexpr uint64_t testTracedProcessDtb = 0xbbb00000;
        constexpr uint64_t testTracedProcessUserDtb = 0xccc00000;
        constexpr uint64_t testOriginalMemoryContent = 0xFE, testOriginalMemoryContent2 = 0xFF;
        constexpr uint64_t expectedR8 = 0x123;
        constexpr uint32_t testVcpuId = 0;
        constinit x86_registers_t x86Regs{
            .r8 = expectedR8,
            .cr3 = testSystemDtb,
        };
    }

    MATCHER_P(IsCorrectMemEvent, expectedGfn, "")
    {
        if (arg.type != VMI_EVENT_MEMORY)
        {
            *result_listener << "\nNot a memory event: type = " << arg.type;
            return false;
        }
        if (expectedGfn != arg.mem_event.gfn)
        {
            *result_listener << "\nTarget gfn not equal:";
            *result_listener << "\nExpected: " << expectedGfn;
            *result_listener << "\nActual: " << arg.mem_event.gfn;
            return false;
        }
        return true;
    }

    class InterruptEventFixture : public testing::Test
    {
      protected:
        std::shared_ptr<MockLibvmiInterface> vmiInterface = std::make_shared<MockLibvmiInterface>();
        std::shared_ptr<MockSingleStepSupervisor> singleStepSupervisor = std::make_shared<MockSingleStepSupervisor>();
        std::shared_ptr<testing::internal::MockFunction<BpResponse(IInterruptEvent&)>> mockBreakpointCallback =
            std::make_shared<testing::MockFunction<BpResponse(IInterruptEvent&)>>();
        std::shared_ptr<NiceMock<MockLogging>> mockLogging = std::make_shared<NiceMock<MockLogging>>();
        std::shared_ptr<InterruptEventSupervisor> interruptEventSupervisor;
        std::shared_ptr<MockActiveProcessesSupervisor> activeProcessesSupervisor =
            std::make_shared<MockActiveProcessesSupervisor>();
        vmi_event_t* interruptSupervisorInternalEvent = nullptr;
        std::shared_ptr<RegisterEventSupervisor> contextSwitchHandler =
            std::make_shared<RegisterEventSupervisor>(vmiInterface, mockLogging);

        std::shared_ptr<ActiveProcessInformation> systemProcessInformation =
            std::make_shared<ActiveProcessInformation>(ActiveProcessInformation{.processDtb = testSystemDtb});
        std::shared_ptr<ActiveProcessInformation> defaultTestProcessInfo =
            std::make_shared<ActiveProcessInformation>(ActiveProcessInformation{
                .processDtb = testTracedProcessDtb,
                .processUserDtb = testTracedProcessUserDtb,
            });

        void setupBreakpoint(addr_t eventVA,
                             addr_t eventPA,
                             addr_t processDtb,
                             uint8_t memoryContent = testOriginalMemoryContent)
        {
            ON_CALL(*vmiInterface, convertVAToPA(eventVA, processDtb)).WillByDefault(Return(eventPA));
            ON_CALL(*vmiInterface, read8PA(eventPA)).WillByDefault(Return(memoryContent));
        }

        vmi_event_t* setupInterruptEvent(addr_t eventVA, addr_t eventPA, x86_regs& regs, uint32_t vcpuId = 0)
        {
            interruptSupervisorInternalEvent->vcpu_id = vcpuId;
            interruptSupervisorInternalEvent->interrupt_event.gla = eventVA;
            interruptSupervisorInternalEvent->interrupt_event.gfn = eventPA >> PagingDefinitions::numberOfPageIndexBits;
            interruptSupervisorInternalEvent->interrupt_event.offset = eventPA & PagingDefinitions::pageOffsetMask;
            interruptSupervisorInternalEvent->x86_regs = &regs;

            return interruptSupervisorInternalEvent;
        }

        void SetUp() override
        {
            ON_CALL(*activeProcessesSupervisor, getSystemProcessInformation())
                .WillByDefault(Return(systemProcessInformation));
            // Required for InterruptGuard
            ON_CALL(*vmiInterface, readXVA(_, _, _, _)).WillByDefault(Return(true));
            ON_CALL(*mockLogging, newNamedLogger(_))
                .WillByDefault([](std::string_view) { return std::make_unique<MockLogger>(); });

            // Gain access to internal interrupt event within InterruptEventSupervisor
            ON_CALL(*vmiInterface, registerEvent(_))
                .WillByDefault(
                    [&interruptSupervisorInternalEvent = interruptSupervisorInternalEvent](vmi_event_t& event)
                    {
                        if (event.type == VMI_EVENT_INTERRUPT)
                        {
                            interruptSupervisorInternalEvent = &event;
                        }
                    });

            interruptEventSupervisor = std::make_shared<InterruptEventSupervisor>(
                vmiInterface, singleStepSupervisor, activeProcessesSupervisor, contextSwitchHandler, mockLogging);
            interruptEventSupervisor->initialize();

            GlobalControl::init(std::make_unique<NiceMock<MockLogger>>(),
                                std::make_shared<NiceMock<MockEventStream>>());
        }

        void TearDown() override
        {
            interruptEventSupervisor->teardown();
            GlobalControl::uninit();
        }
    };

    TEST_F(InterruptEventFixture, createBreakpoint_int3AtTargetPA_vmiException)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, INT3_BREAKPOINT);

        EXPECT_THROW(interruptEventSupervisor->createBreakpoint(
                         testKernelVA1, *systemProcessInformation, breakpointCallbackFunction_t{}, true),
                     VmiException);
    }

    TEST_F(InterruptEventFixture, createBreakpoint_validParameters_doesNotThrow)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        EXPECT_CALL(*vmiInterface, write8PA(_, _)).Times(AnyNumber());
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, INT3_BREAKPOINT)).Times(1).RetiresOnSaturation();

        EXPECT_NO_THROW(interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true));
    }

    TEST_F(InterruptEventFixture, createBreakpoint_newInt3OnNewPage_guardCreatedBeforeInt3Write)
    {
        testing::Sequence s1;
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        EXPECT_CALL(*vmiInterface,
                    readXVA(testKernelVA1, systemProcessInformation->processDtb, _, PagingDefinitions::pageSizeInBytes))
            .Times(1)
            .InSequence(s1)
            .RetiresOnSaturation();
        EXPECT_CALL(
            *vmiInterface,
            readXVA(testKernelVA1 + PagingDefinitions::pageSizeInBytes, systemProcessInformation->processDtb, _, 16))
            .Times(1)
            .InSequence(s1)
            .RetiresOnSaturation();
        EXPECT_CALL(*vmiInterface, write8PA(_, _)).Times(AnyNumber());
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, INT3_BREAKPOINT)).Times(1).InSequence(s1).RetiresOnSaturation();

        EXPECT_NO_THROW(interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true));
    }

    TEST_F(InterruptEventFixture, _defaultInterruptCallback_twoEventsRegistered_bothCallbacksCalled)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        auto occupantMockFunction = std::make_shared<testing::MockFunction<BpResponse(IInterruptEvent&)>>();
        auto _occupant = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, occupantMockFunction->AsStdFunction(), false);
        auto conflictingInterruptMockFunction = std::make_shared<testing::MockFunction<BpResponse(IInterruptEvent&)>>();
        auto _conflictingInterrupt = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, conflictingInterruptMockFunction->AsStdFunction(), false);

        EXPECT_CALL(*occupantMockFunction, Call(_)).Times(1);
        EXPECT_CALL(*conflictingInterruptMockFunction, Call(_)).Times(1);
        auto* interruptEvent = setupInterruptEvent(testKernelVA1, testPA1, x86Regs);

        InterruptEventSupervisor::_defaultInterruptCallback(vmiInstanceStub, interruptEvent);
    }

    TEST_F(InterruptEventFixture, teardown_twoDistinctEventsRegisteredOnSamePage_doesNotThrow)
    {
        ON_CALL(*vmiInterface, convertVAToPA(testKernelVA1, systemProcessInformation->processDtb))
            .WillByDefault(Return(testPA1));
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, breakpointCallbackFunction_t{}, true);
        ON_CALL(*vmiInterface, convertVAToPA(testKernelVA1 + 1, systemProcessInformation->processDtb))
            .WillByDefault(Return(testPA1 + 1));
        auto breakpoint2 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1 + 1, *systemProcessInformation, breakpointCallbackFunction_t{}, true);

        EXPECT_NO_THROW(interruptEventSupervisor->teardown());
    }

    TEST_F(InterruptEventFixture, _defaultInterruptCallback_eventReadingR8_CorrectContent)
    {
        uint64_t result = 0;
        auto testCallbackReadsR8 = [&result](IInterruptEvent& event)
        {
            result = event.getR8();
            return BpResponse::Continue;
        };
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        auto _event = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, testCallbackReadsR8, true);
        auto* interruptEvent = setupInterruptEvent(testKernelVA1, testPA1, x86Regs);

        InterruptEventSupervisor::_defaultInterruptCallback(vmiInstanceStub, interruptEvent);

        EXPECT_EQ(expectedR8, result);
    }

    TEST_F(InterruptEventFixture, _defaultInterruptCallback_nonRegisteredPA_reinjectsEvent)
    {
        uint64_t unknownVA = 0;
        uint64_t unknownPA = 0;
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        auto _interruptEvent = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        auto* interruptEvent = setupInterruptEvent(unknownVA, unknownPA, x86Regs);

        EXPECT_NO_THROW(InterruptEventSupervisor::_defaultInterruptCallback(vmiInstanceStub, interruptEvent));

        EXPECT_EQ(interruptEvent->interrupt_event.reinject, REINJECT_INTERRUPT);
    }

    TEST_F(InterruptEventFixture, _defaultInterruptCallback_interruptEventTriggered_disablesEvent)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, testOriginalMemoryContent);
        auto _breakpoint = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        EXPECT_CALL(*vmiInterface, write8PA(_, _)).Times(AnyNumber());
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, testOriginalMemoryContent)).Times(1).RetiresOnSaturation();
        auto* interruptEvent = setupInterruptEvent(testKernelVA1, testPA1, x86Regs);

        EXPECT_NO_THROW(InterruptEventSupervisor::_defaultInterruptCallback(vmiInstanceStub, interruptEvent));
    }

    TEST_F(InterruptEventFixture, _defaultInterruptCallback_singleStepTriggered_reenablesEvent)
    {
        vmi_event_t singleStepEvent{
            .data = reinterpret_cast<void*>(testPA1),
            .vcpu_id = testVcpuId,
        };
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        auto _breakpoint = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        singleStepCallbackFunction_t singleStepCallback;
        auto* interruptEvent = setupInterruptEvent(testKernelVA1, testPA1, x86Regs, testVcpuId);
        EXPECT_CALL(*singleStepSupervisor, setSingleStepCallback(testVcpuId, _, _))
            .WillOnce(SaveArg<1>(&singleStepCallback));
        EXPECT_CALL(*vmiInterface, write8PA(_, _)).Times(AnyNumber());
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, INT3_BREAKPOINT)).Times(1).RetiresOnSaturation();

        EXPECT_NO_THROW(InterruptEventSupervisor::_defaultInterruptCallback(vmiInstanceStub, interruptEvent));
        ASSERT_TRUE(singleStepCallback);
        EXPECT_NO_THROW(singleStepCallback(&singleStepEvent));
    }

    TEST_F(InterruptEventFixture, _defaultInterruptCallback_interruptEventTriggered_returnsEventResponseNone)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        auto _breakpoint = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        auto* interruptEvent = setupInterruptEvent(testKernelVA1, testPA1, x86Regs);

        EXPECT_EQ(InterruptEventSupervisor::_defaultInterruptCallback(vmiInstanceStub, interruptEvent),
                  VMI_EVENT_RESPONSE_NONE);
    }

    class InterruptEventFixtureWithoutInterruptEventSupervisorTeardown : public InterruptEventFixture
    {
        void TearDown() override
        {
            GlobalControl::uninit();
        }
    };

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown, teardown_singleRemovedInterrupt_noThrow)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, testOriginalMemoryContent);
        auto breakpoint = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);

        ASSERT_NO_THROW(breakpoint->remove());

        EXPECT_NO_THROW(interruptEventSupervisor->teardown());
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown, teardown_activeInterrupt_vmPausedAndResumed)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, testOriginalMemoryContent);
        auto _breakpoint = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        EXPECT_CALL(*vmiInterface, write8PA(_, _)).Times(AnyNumber());
        testing::Sequence s1;
        EXPECT_CALL(*vmiInterface, pauseVm()).Times(1).InSequence(s1);
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, testOriginalMemoryContent)).Times(1).InSequence(s1);
        EXPECT_CALL(*vmiInterface, resumeVm()).Times(1).InSequence(s1);

        interruptEventSupervisor->teardown();
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           teardown_twoActiveInterrupts_interruptsDisabled)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, testOriginalMemoryContent);
        setupBreakpoint(testKernelVA2, testPA2, systemProcessInformation->processDtb, testOriginalMemoryContent2);
        auto _breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        auto _breakpoint2 = interruptEventSupervisor->createBreakpoint(
            testKernelVA2, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        EXPECT_CALL(*vmiInterface, write8PA(_, _)).Times(AnyNumber());
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, testOriginalMemoryContent)).Times(1);
        EXPECT_CALL(*vmiInterface, write8PA(testPA2, testOriginalMemoryContent2)).Times(1);

        interruptEventSupervisor->teardown();
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           teardown_twoActiveInterrupts_interruptGuardsDisabled)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        setupBreakpoint(testKernelVA2, testPA2, systemProcessInformation->processDtb);
        auto _breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        auto _breakpoint2 = interruptEventSupervisor->createBreakpoint(
            testKernelVA2, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        EXPECT_CALL(*vmiInterface, clearEvent(_, _)).Times(AnyNumber());
        EXPECT_CALL(*vmiInterface,
                    clearEvent(IsCorrectMemEvent(testPA1 >> PagingDefinitions::numberOfPageIndexBits), false))
            .Times(1);
        EXPECT_CALL(*vmiInterface,
                    clearEvent(IsCorrectMemEvent(testPA2 >> PagingDefinitions::numberOfPageIndexBits), false))
            .Times(1);

        interruptEventSupervisor->teardown();
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           deleteBreakpoint_breakpointRemovedTwoTimes_noThrow)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, testOriginalMemoryContent);
        auto breakpoint = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        breakpoint->remove();

        EXPECT_NO_THROW(breakpoint->remove());
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           deleteBreakpoint_noPendingEvents_eventsListenNotCalled)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, testOriginalMemoryContent);
        auto breakpoint = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        ON_CALL(*vmiInterface, areEventsPending()).WillByDefault(Return(0));
        EXPECT_CALL(*vmiInterface, eventsListen(_)).Times(0);

        interruptEventSupervisor->deleteBreakpoint(breakpoint.get());
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           deleteBreakpoint_pendingEventsPresent_eventsListenCalled)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, testOriginalMemoryContent);
        auto breakpoint = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        ON_CALL(*vmiInterface, areEventsPending()).WillByDefault(Return(1));
        EXPECT_CALL(*vmiInterface, eventsListen(_)).Times(1);

        interruptEventSupervisor->deleteBreakpoint(breakpoint.get());
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           deleteBreakpoint_twoBreakpointsOnSameAddress_interruptNotOverwrittenInMemory)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, testOriginalMemoryContent);
        EXPECT_CALL(*vmiInterface, write8PA(_, _)).Times(AnyNumber());
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        auto _breakpoint2 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, testOriginalMemoryContent)).Times(0);

        interruptEventSupervisor->deleteBreakpoint(breakpoint1.get());
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           deleteBreakpoint_twoBreakpointsOnSameAddress_VmNotPaused)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb, testOriginalMemoryContent);
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        auto _breakpoint2 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        EXPECT_CALL(*vmiInterface, pauseVm()).Times(0);
        EXPECT_CALL(*vmiInterface, resumeVm()).Times(0);

        interruptEventSupervisor->deleteBreakpoint(breakpoint1.get());
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           deleteBreakpoint_twoBreakpointsOnSameAddressRemoved_interruptRemovedFromMemory)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        EXPECT_CALL(*vmiInterface, write8PA(_, _)).Times(AnyNumber());
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        auto breakpoint2 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, testOriginalMemoryContent)).Times(1);

        interruptEventSupervisor->deleteBreakpoint(breakpoint1.get());
        interruptEventSupervisor->deleteBreakpoint(breakpoint2.get());
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           _defaultContextSwitchCallback_processExclusiveBpInSystemContextSwitch_bPDisabled)
    {
        setupBreakpoint(testUserVA1, testPA2, defaultTestProcessInfo->processUserDtb);
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testUserVA1, *defaultTestProcessInfo, mockBreakpointCallback->AsStdFunction(), false);
        interruptSupervisorInternalEvent->reg_event.value = testSystemDtb;
        EXPECT_CALL(*vmiInterface, write8PA(testPA2, testOriginalMemoryContent)).Times(1);

        interruptEventSupervisor->contextSwitchCallback(interruptSupervisorInternalEvent);
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           _defaultContextSwitchCallback_globalBpInSystemContextSwitch_bpUnchanged)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        interruptSupervisorInternalEvent->reg_event.value = systemProcessInformation->processDtb;
        EXPECT_CALL(*vmiInterface, write8PA(_, _)).Times(0);

        interruptEventSupervisor->contextSwitchCallback(interruptSupervisorInternalEvent);
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           _defaultContextSwitchCallback_twoProcessExclusiveBpInSystemContextSwitch_BothBpDisabled)
    {
        ActiveProcessInformation secondTestProcess{.processUserDtb = 0x666000};
        setupBreakpoint(testUserVA1, testPA1, defaultTestProcessInfo->processUserDtb);
        setupBreakpoint(testUserVA2, testPA2, secondTestProcess.processUserDtb);
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testUserVA1, *defaultTestProcessInfo, mockBreakpointCallback->AsStdFunction(), false);
        auto breakpoint2 = interruptEventSupervisor->createBreakpoint(
            testUserVA2, secondTestProcess, mockBreakpointCallback->AsStdFunction(), false);
        interruptSupervisorInternalEvent->reg_event.value = testSystemDtb;
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, testOriginalMemoryContent)).Times(1);
        EXPECT_CALL(*vmiInterface, write8PA(testPA2, testOriginalMemoryContent)).Times(1);

        interruptEventSupervisor->contextSwitchCallback(interruptSupervisorInternalEvent);
        interruptEventSupervisor->contextSwitchCallback(interruptSupervisorInternalEvent);
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           _defaultContextSwitchCallback_oneGlobalOneProcessExclusiveBpInSystemContextSwitch_processExclusiveBpDisabled)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        setupBreakpoint(testUserVA1, testPA2, defaultTestProcessInfo->processUserDtb);
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        auto breakpoint2 = interruptEventSupervisor->createBreakpoint(
            testUserVA1, *defaultTestProcessInfo, mockBreakpointCallback->AsStdFunction(), false);
        interruptSupervisorInternalEvent->reg_event.value = systemProcessInformation->processDtb;
        EXPECT_CALL(*vmiInterface, write8PA(testPA1, testOriginalMemoryContent)).Times(0);
        EXPECT_CALL(*vmiInterface, write8PA(testPA2, testOriginalMemoryContent)).Times(1);

        interruptEventSupervisor->contextSwitchCallback(interruptSupervisorInternalEvent);
    }

    TEST_F(InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
           _defaultContextSwitchCallback_OneProcessExclusiveBpInTargetProcessContextSwitch_BpEnabled)
    {
        setupBreakpoint(testUserVA1, testPA2, defaultTestProcessInfo->processUserDtb);
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testUserVA1, *defaultTestProcessInfo, mockBreakpointCallback->AsStdFunction(), false);
        // Simulating being in another context
        interruptSupervisorInternalEvent->reg_event.value = testSystemDtb;
        interruptEventSupervisor->contextSwitchCallback(interruptSupervisorInternalEvent);
        interruptSupervisorInternalEvent->reg_event.value = defaultTestProcessInfo->processUserDtb;
        EXPECT_CALL(*vmiInterface, write8PA(testPA2, INT3_BREAKPOINT)).Times(1);

        interruptEventSupervisor->contextSwitchCallback(interruptSupervisorInternalEvent);
    }

    TEST_F(
        InterruptEventFixtureWithoutInterruptEventSupervisorTeardown,
        _defaultContextSwitchCallback_oneGlobalOneProcessExclusiveBpInTargetProcessContextSwitch_processExclusiveBpEnabledGlobalUnchanged)
    {
        setupBreakpoint(testKernelVA1, testPA1, systemProcessInformation->processDtb);
        setupBreakpoint(testUserVA1, testPA2, defaultTestProcessInfo->processUserDtb);
        auto breakpoint1 = interruptEventSupervisor->createBreakpoint(
            testKernelVA1, *systemProcessInformation, mockBreakpointCallback->AsStdFunction(), true);
        auto breakpoint2 = interruptEventSupervisor->createBreakpoint(
            testUserVA1, *defaultTestProcessInfo, mockBreakpointCallback->AsStdFunction(), false);
        // Simulating being in another context
        interruptSupervisorInternalEvent->reg_event.value = systemProcessInformation->processDtb;
        interruptEventSupervisor->contextSwitchCallback(interruptSupervisorInternalEvent);
        interruptSupervisorInternalEvent->reg_event.value = defaultTestProcessInfo->processUserDtb;
        EXPECT_CALL(*vmiInterface, write8PA(testPA2, INT3_BREAKPOINT)).Times(1);

        interruptEventSupervisor->contextSwitchCallback(interruptSupervisorInternalEvent);
    }
}
