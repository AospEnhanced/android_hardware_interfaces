/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "neuralnetworks_hidl_hal_test"

#include "VtsHalNeuralnetworks.h"
#include "1.0/Callbacks.h"
#include "1.0/Utils.h"
#include "GeneratedTestHarness.h"
#include "TestHarness.h"

#include <android-base/logging.h>

namespace android::hardware::neuralnetworks::V1_1::vts::functional {

using V1_0::ErrorStatus;
using V1_0::IPreparedModel;
using V1_0::Request;
using V1_0::implementation::PreparedModelCallback;

static void createPreparedModel(const sp<IDevice>& device, const Model& model,
                                sp<IPreparedModel>* preparedModel) {
    ASSERT_NE(nullptr, preparedModel);

    // see if service can handle model
    bool fullySupportsModel = false;
    Return<void> supportedOpsLaunchStatus = device->getSupportedOperations_1_1(
            model, [&fullySupportsModel](ErrorStatus status, const hidl_vec<bool>& supported) {
                ASSERT_EQ(ErrorStatus::NONE, status);
                ASSERT_NE(0ul, supported.size());
                fullySupportsModel = std::all_of(supported.begin(), supported.end(),
                                                 [](bool valid) { return valid; });
            });
    ASSERT_TRUE(supportedOpsLaunchStatus.isOk());

    // launch prepare model
    sp<PreparedModelCallback> preparedModelCallback = new PreparedModelCallback();
    Return<ErrorStatus> prepareLaunchStatus = device->prepareModel_1_1(
            model, ExecutionPreference::FAST_SINGLE_ANSWER, preparedModelCallback);
    ASSERT_TRUE(prepareLaunchStatus.isOk());
    ASSERT_EQ(ErrorStatus::NONE, static_cast<ErrorStatus>(prepareLaunchStatus));

    // retrieve prepared model
    preparedModelCallback->wait();
    ErrorStatus prepareReturnStatus = preparedModelCallback->getStatus();
    *preparedModel = preparedModelCallback->getPreparedModel();

    // The getSupportedOperations_1_1 call returns a list of operations that are
    // guaranteed not to fail if prepareModel_1_1 is called, and
    // 'fullySupportsModel' is true i.f.f. the entire model is guaranteed.
    // If a driver has any doubt that it can prepare an operation, it must
    // return false. So here, if a driver isn't sure if it can support an
    // operation, but reports that it successfully prepared the model, the test
    // can continue.
    if (!fullySupportsModel && prepareReturnStatus != ErrorStatus::NONE) {
        ASSERT_EQ(nullptr, preparedModel->get());
        LOG(INFO) << "NN VTS: Unable to test Request validation because vendor service cannot "
                     "prepare model that it does not support.";
        std::cout << "[          ]   Unable to test Request validation because vendor service "
                     "cannot prepare model that it does not support."
                  << std::endl;
        return;
    }
    ASSERT_EQ(ErrorStatus::NONE, prepareReturnStatus);
    ASSERT_NE(nullptr, preparedModel->get());
}

// A class for test environment setup
NeuralnetworksHidlEnvironment* NeuralnetworksHidlEnvironment::getInstance() {
    // This has to return a "new" object because it is freed inside
    // ::testing::AddGlobalTestEnvironment when the gtest is being torn down
    static NeuralnetworksHidlEnvironment* instance = new NeuralnetworksHidlEnvironment();
    return instance;
}

void NeuralnetworksHidlEnvironment::registerTestServices() {
    registerTestService<IDevice>();
}

// The main test class for NEURALNETWORK HIDL HAL.
void NeuralnetworksHidlTest::SetUp() {
    ::testing::VtsHalHidlTargetTestBase::SetUp();

#ifdef PRESUBMIT_NOT_VTS
    const std::string name =
            NeuralnetworksHidlEnvironment::getInstance()->getServiceName<IDevice>();
    const std::string sampleDriver = "sample-";
    if (device == nullptr && name.substr(0, sampleDriver.size()) == sampleDriver) {
        GTEST_SKIP();
    }
#endif  // PRESUBMIT_NOT_VTS

    ASSERT_NE(nullptr, device.get());
}

void NeuralnetworksHidlTest::TearDown() {
    ::testing::VtsHalHidlTargetTestBase::TearDown();
}

void ValidationTest::validateEverything(const Model& model, const Request& request) {
    validateModel(model);

    // create IPreparedModel
    sp<IPreparedModel> preparedModel;
    ASSERT_NO_FATAL_FAILURE(createPreparedModel(device, model, &preparedModel));
    if (preparedModel == nullptr) {
        return;
    }

    validateRequest(preparedModel, request);
}

TEST_P(ValidationTest, Test) {
    const Model model = createModel(*mTestModel);
    const Request request = createRequest(*mTestModel);
    ASSERT_FALSE(mTestModel->expectFailure);
    validateEverything(model, request);
}

INSTANTIATE_GENERATED_TEST(ValidationTest, [](const test_helper::TestModel&) { return true; });

}  // namespace android::hardware::neuralnetworks::V1_1::vts::functional

namespace android::hardware::neuralnetworks::V1_0 {

::std::ostream& operator<<(::std::ostream& os, ErrorStatus errorStatus) {
    return os << toString(errorStatus);
}

::std::ostream& operator<<(::std::ostream& os, DeviceStatus deviceStatus) {
    return os << toString(deviceStatus);
}

}  // namespace android::hardware::neuralnetworks::V1_0

using android::hardware::neuralnetworks::V1_1::vts::functional::NeuralnetworksHidlEnvironment;

int main(int argc, char** argv) {
    ::testing::AddGlobalTestEnvironment(NeuralnetworksHidlEnvironment::getInstance());
    ::testing::InitGoogleTest(&argc, argv);
    NeuralnetworksHidlEnvironment::getInstance()->init(&argc, argv);

    int status = RUN_ALL_TESTS();
    return status;
}
