#pragma once

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace leica_watermark {

inline constexpr char kTemplateRoot[] = "/data/vendor/camera/watermarks/leica";
inline constexpr char kLegacyModel[] = "Turbo 4 Pro";
inline constexpr char kGlobalModel[] = "F7";

inline bool IsChinaRegion() {
    char hwc[PROP_VALUE_MAX] = {};
    return __system_property_get("ro.boot.hwc", hwc) > 0 && std::strcmp(hwc, "CN") == 0;
}

inline bool IsLeicaTemplatePath(const std::string& path) {
    const std::string prefix = std::string(kTemplateRoot) + "/";
    return path.compare(0, prefix.size(), prefix) == 0 &&
           path.size() >= 5 && path.compare(path.size() - 5, 5, ".json") == 0;
}

inline size_t ReplaceModel(std::string* data) {
    size_t replacements = 0;
    size_t at = 0;
    while ((at = data->find(kLegacyModel, at)) != std::string::npos) {
        data->replace(at, sizeof(kLegacyModel) - 1, kGlobalModel);
        at += sizeof(kGlobalModel) - 1;
        ++replacements;
    }
    return replacements;
}

inline size_t NormalizeTemplate(const std::string& path, std::string* data) {
    if (IsChinaRegion() || !IsLeicaTemplatePath(path)) return 0;
    return ReplaceModel(data);
}

inline bool WriteAll(int fd, const char* data, size_t size) {
    size_t written = 0;
    while (written < size) {
        ssize_t count = write(fd, data + written, size - written);
        if (count <= 0) return false;
        written += static_cast<size_t>(count);
    }
    return true;
}

inline int RewriteExistingTemplates(const std::string& path = kTemplateRoot) {
    if (IsChinaRegion()) return 0;
    DIR* dir = opendir(path.c_str());
    if (!dir) return 0;

    int repaired = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        const std::string fullPath = path + "/" + entry->d_name;
        struct stat st;
        if (lstat(fullPath.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            repaired += RewriteExistingTemplates(fullPath);
            continue;
        }
        if (!S_ISREG(st.st_mode) || !IsLeicaTemplatePath(fullPath)) continue;

        int input = open(fullPath.c_str(), O_RDONLY);
        if (input < 0) continue;
        std::string data;
        char buffer[8192];
        ssize_t count;
        while ((count = read(input, buffer, sizeof(buffer))) > 0) data.append(buffer, count);
        close(input);
        if (count < 0 || NormalizeTemplate(fullPath, &data) == 0) continue;

        const std::string tempPath = fullPath + ".tmp";
        int output = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
        if (output < 0) continue;
        fchmod(output, st.st_mode & 0777);
        const bool written = WriteAll(output, data.data(), data.size());
        close(output);
        if (written && rename(tempPath.c_str(), fullPath.c_str()) == 0) {
            ++repaired;
        } else {
            unlink(tempPath.c_str());
        }
    }
    closedir(dir);
    return repaired;
}

}  // namespace leica_watermark