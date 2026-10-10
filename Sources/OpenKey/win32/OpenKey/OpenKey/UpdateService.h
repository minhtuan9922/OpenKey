#pragma once
#include "UpdateCheckService.h"
#include "UpdateDownloadService.h"

namespace UpdateService {
struct Outcome {
    UpdateCheckService::Result result = UpdateCheckService::Result::DownloadFailed;
    std::string version;
};
// currentVersion is legacy packed major | minor<<8 | patch<<16, not ordered.
template<class Api> Outcome Check(uint32_t currentVersion, Api& api) {
    Outcome outcome;
    std::wstring data;
    const auto download = UpdateDownloadService::Download(api, data);
    if (download == UpdateDownloadService::Result::Success)
        outcome.result = UpdateCheckService::Check(data, currentVersion, outcome.version);
    else if (download == UpdateDownloadService::Result::Invalid)
        outcome.result = UpdateCheckService::Result::InvalidMetadata;
    return outcome;
}
inline Outcome Check(uint32_t currentVersion) {
    UpdateDownloadService::NativeApi api;
    return Check(currentVersion, api);
}
}
