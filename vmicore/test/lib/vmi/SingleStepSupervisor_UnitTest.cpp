#include "../io/grpc/mock_GRPCLogger.h"
#include "../io/mock_EventStream.h"
#include "../io/mock_Logging.h"
#include "mock_LibvmiInterface.h"
#include <GlobalControl.h>
#include <gtest/gtest.h>
#include <vmi/SingleStepSupervisor.h>
#include <vmi/VmiException.h>

using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;

namespace VmiCore
{
    TEST(SingleStepSupervisorTest, constructor_multipleInstances_throwsRuntimeError)
    {
        std::shared_ptr<NiceMock<MockLogging>> mockLogging = std::make_shared<NiceMock<MockLogging>>();
        ON_CALL(*mockLogging, newNamedLogger(_))
            .WillByDefault([](const std::string_view&) { return std::make_unique<MockGRPCLogger>(); });

        std::shared_ptr<ILibvmiInterface> vmiInterface = std::make_shared<MockLibvmiInterface>();
        SingleStepSupervisor firstInstance(vmiInterface, mockLogging);

        EXPECT_THROW(SingleStepSupervisor secondInstance(vmiInterface, mockLogging), std::runtime_error);
    }

    class SingleStepSupvervisorValidStateFixture : public testing::Test
    {
      protected:
        using singleStepFunction_t = std::function<void(vmi_event_t*)>;

        std::shared_ptr<MockLibvmiInterface> vmiInterface = std::make_shared<MockLibvmiInterface>();
        std::unique_ptr<SingleStepSupervisor> singleStepSupervisor;
        std::shared_ptr<testing::internal::MockFunction<void(vmi_event_t*)>> mockFunction =
            std::make_shared<testing::MockFunction<void(vmi_event_t*)>>();
        std::shared_ptr<NiceMock<MockLogging>> mockLogging = std::make_shared<NiceMock<MockLogging>>();
        singleStepFunction_t callbackFunction = SingleStepSupervisor::createSingleStepCallback(
            mockFunction, &testing::MockFunction<void(vmi_event_t*)>::Call);
        uint numberOfTestVcpus = 1;
        uint testVcpuId = 0;

        void SetUp() override
        {
            ON_CALL(*mockLogging, newNamedLogger(_))
                .WillByDefault([](const std::string_view&) { return std::make_unique<MockGRPCLogger>(); });

            ON_CALL(*vmiInterface, getNumberOfVCPUs()).WillByDefault(Return(numberOfTestVcpus));
            singleStepSupervisor = std::make_unique<SingleStepSupervisor>(vmiInterface, mockLogging);
            singleStepSupervisor->initializeSingleStepEvents();

            GlobalControl::init(std::make_unique<NiceMock<MockGRPCLogger>>(),
                                std::make_shared<NiceMock<MockEventStream>>());
        }

        void TearDown() override
        {
            GlobalControl::uninit();
        }
    };

    TEST_F(SingleStepSupvervisorValidStateFixture, setSingleStepCallback_callbackTargetExpired_doesNotThrow)
    {
        singleStepSupervisor->setSingleStepCallback(testVcpuId, callbackFunction, 0);
        mockFunction.reset();
        vmi_event_t testEvent{};
        testEvent.vcpu_id = testVcpuId;

        EXPECT_NO_THROW(SingleStepSupervisor::_defaultSingleStepCallback(0, &testEvent));
    }

    TEST_F(SingleStepSupvervisorValidStateFixture, setSingleStepCallback_validCallbackTarget_triggersCallback)
    {
        singleStepSupervisor->setSingleStepCallback(testVcpuId, callbackFunction, 0);
        vmi_event_t testEvent{};
        testEvent.vcpu_id = testVcpuId;

        EXPECT_CALL(*mockFunction, Call(_)).Times(1);
        EXPECT_NO_THROW(SingleStepSupervisor::_defaultSingleStepCallback(0, &testEvent));
    }

    TEST_F(SingleStepSupvervisorValidStateFixture,
           setSingleStepCallback_callbackAlreadyRegisteredForCurrentVcpu_throwsVmiException)
    {
        singleStepSupervisor->setSingleStepCallback(testVcpuId, callbackFunction, 0);

        EXPECT_THROW(singleStepSupervisor->setSingleStepCallback(testVcpuId, callbackFunction, 0), VmiException);
    }

    TEST_F(SingleStepSupvervisorValidStateFixture, setSingleStepCallback_validCallbackTarget_stopSingleStepForVCPU)
    {
        singleStepSupervisor->setSingleStepCallback(testVcpuId, callbackFunction, 0);
        vmi_event_t testEvent{};
        testEvent.vcpu_id = testVcpuId;

        EXPECT_CALL(*vmiInterface, stopSingleStepForVcpu(&testEvent, testVcpuId)).Times(AtLeast(1));
        EXPECT_NO_THROW(SingleStepSupervisor::_defaultSingleStepCallback(0, &testEvent));
    }
}