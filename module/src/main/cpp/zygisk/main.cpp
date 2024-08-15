#include <android/log.h>
#include <vector>
#include <string>
#include <filesystem>

#include "logging.hpp"
#include "zygisk.hpp"
#include "cJSON.h"

#define JSON_PATH "/data/adb/pif.json"
#define JSON_TS_PATH "/data/adb/tricky_store/pif.json"
#define JSON_PIF_PATH "/data/adb/playintegrityfix/pif.json"
#define JSON_PIF_FORK_PATH "/data/adb/modules/playintegrityfix/custom.pif.json"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

static ssize_t xread(int fd, void *buffer, size_t count) {
    ssize_t total = 0;
    char *buf = (char *) buffer;
    while (count > 0) {
        ssize_t ret = read(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static ssize_t xwrite(int fd, const void *buffer, size_t count) {
    ssize_t total = 0;
    char *buf = (char *) buffer;
    while (count > 0) {
        ssize_t ret = write(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

class TrickyStore : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api_ = api;
        this->env_ = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        if (!args) return;

        const char *dir = env_->GetStringUTFChars(args->app_data_dir, nullptr);

        if (!dir) return;

        bool isGms = std::string_view(dir).ends_with("/com.google.android.gms");

        env_->ReleaseStringUTFChars(args->app_data_dir, dir);

        if (!isGms) return;

        api_->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        const char *name = env_->GetStringUTFChars(args->nice_name, nullptr);

        if (!name) return;

        bool isGmsUnstable = std::string_view(name) == "com.google.android.gms.unstable";

        env_->ReleaseStringUTFChars(args->nice_name, name);

        if (!isGmsUnstable) return;

        size_t jsonVectorSize;
        std::vector<char> temp;

        int fd = api_->connectCompanion();

        xread(fd, &jsonVectorSize, sizeof(jsonVectorSize));
        temp.resize(jsonVectorSize);
        xread(fd, temp.data(), jsonVectorSize);

        close(fd);

        if (temp.empty()) {
            LOGI("Couldn't receive JSON file from fd!");
            return;
        }

        std::string jsonString(temp.cbegin(), temp.cend());
        json = cJSON_ParseWithLength(jsonString.c_str(), jsonVectorSize);
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (!json) return;

        UpdateBuildFields();

        cJSON_Delete(json);
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api_{nullptr};
    JNIEnv *env_{nullptr};
    cJSON *json{nullptr};

    void UpdateBuildFields() {
        jclass buildClass = env_->FindClass("android/os/Build");
        jclass versionClass = env_->FindClass("android/os/Build$VERSION");

        cJSON *currentElement = nullptr;
        cJSON_ArrayForEach(currentElement, json) {
            const char *key = currentElement->string;

            if (cJSON_IsString(currentElement)) {
                const char *value = currentElement->valuestring;
                jfieldID fieldID = env_->GetStaticFieldID(buildClass, key, "Ljava/lang/String;");

                if (env_->ExceptionCheck()) {
                    env_->ExceptionClear();

                    fieldID = env_->GetStaticFieldID(versionClass, key, "Ljava/lang/String;");

                    if (env_->ExceptionCheck()) {
                        env_->ExceptionClear();
                        continue;
                    }
                }

                if (fieldID != nullptr) {
                    jstring jValue = env_->NewStringUTF(value);

                    env_->SetStaticObjectField(buildClass, fieldID, jValue);
                    if (env_->ExceptionCheck()) {
                        env_->ExceptionClear();
                        continue;
                    }

                    LOGI("Set '%s' to '%s'", key, value);
                }
            } else if (cJSON_IsNumber(currentElement)) {
                int value = currentElement->valueint;
                jfieldID fieldID = env_->GetStaticFieldID(buildClass, key, "I");

                if (env_->ExceptionCheck()) {
                    env_->ExceptionClear();

                    fieldID = env_->GetStaticFieldID(versionClass, key, "I");

                    if (env_->ExceptionCheck()) {
                        env_->ExceptionClear();
                        continue;
                    }
                }

                if (fieldID != nullptr) {
                    env_->SetStaticIntField(buildClass, fieldID, value);

                    if (env_->ExceptionCheck()) {
                        env_->ExceptionClear();
                        continue;
                    }

                    LOGI("Set '%s' to '%d'", key, value);
                }
            }
        }
    }
};

static std::vector<char> readFile(const char *str) {
    FILE *file = fopen(str, "r");

    if (!file) return {};

    auto fileSize = std::filesystem::file_size(str);

    std::vector<char> vector(fileSize);

    fread(vector.data(), fileSize, 1, file);

    fclose(file);

    LOGI("[companion_handler] file '%s', size: %ld", str, fileSize);

    return vector;
}

static void companion_handler(int fd) {
    std::vector<char> jsonVector;

    jsonVector = readFile(JSON_PATH);
    if (jsonVector.empty()) jsonVector = readFile(JSON_TS_PATH);
    if (jsonVector.empty()) jsonVector = readFile(JSON_PIF_PATH);
    if (jsonVector.empty()) jsonVector = readFile(JSON_PIF_FORK_PATH);

    size_t jsonVectorSize = jsonVector.size();

    xwrite(fd, &jsonVectorSize, sizeof(jsonVectorSize));
    xwrite(fd, jsonVector.data(), jsonVectorSize);
}

// Register our module class and the companion handler function
REGISTER_ZYGISK_MODULE(TrickyStore)

REGISTER_ZYGISK_COMPANION(companion_handler)
