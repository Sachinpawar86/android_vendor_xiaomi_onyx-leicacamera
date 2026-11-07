#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <sys/stat.h>
#include "MiSysImpl.h"
#include "LeicaWatermarkModel.h"
#include "HypSysIntl.h"

using aidl::vendor::xiaomi::hardware::misys::common::MiSysImpl;
using aidl::vendor::xiaomi::hardware::misys::common::MakeCameraDataReadable;
using aidl::vendor::xiaomi::hardware::misys::common::HypSysIntlImpl;

int main() {
    // init forks every service with a hardcoded umask(077), which would turn the
    // 0664/0775 modes MiSysImpl asks for into 0600/0700 - cameraserver could then
    // not read the materialised watermark templates and the watermark would be
    // silently dropped from the saved JPEG.
    umask(0022);

    const int watermarkTemplates = leica_watermark::RewriteExistingTemplates();
    if (watermarkTemplates > 0) {
        LOG(INFO) << "misys: updated " << watermarkTemplates
                  << " Leica watermark templates for the non-China region";
    }

    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();

    // IMiSysImpl
    auto misysService = ndk::SharedRefBase::make<MiSysImpl>();
    const std::string misysName = std::string(MiSysImpl::descriptor) + "/default";
    binder_status_t status1 = AServiceManager_addService(misysService->asBinder().get(), misysName.c_str());
    CHECK_EQ(status1, STATUS_OK) << "Failed to register " << misysName;

    // IHypSysIntl
    auto hypSysService = ndk::SharedRefBase::make<HypSysIntlImpl>();
    const std::string hypSysName = std::string(HypSysIntlImpl::descriptor) + "/default";
    binder_status_t status2 = AServiceManager_addService(hypSysService->asBinder().get(), hypSysName.c_str());
    CHECK_EQ(status2, STATUS_OK) << "Failed to register " << hypSysName;

    LOG(INFO) << "Registered IMiSysImpl and IHypSysIntl successfully";

    LOG(INFO) << "misys: repaired " << MakeCameraDataReadable("/data/vendor/camera", 8)
              << " entries an older build left unreadable for cameraserver";

    ABinderProcess_joinThreadPool();
    return 0;
}
