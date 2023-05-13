/* Copyright 2022-2023 John "topjohnwu" Wu
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

#include "logging.h"
#include "server.h"
#include "zygisk.hpp"
#include <string>

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

using namespace std;

static jstring (*orig_native_get)(JNIEnv *env, jclass clazz, jstring keyJ, jstring defJ);

static jstring my_native_get(JNIEnv *env, jclass clazz, jstring keyJ, jstring defJ) {
    const char *key = env->GetStringUTFChars(keyJ, nullptr);
    const char *def = env->GetStringUTFChars(defJ, nullptr);

    jstring hooked_result = nullptr;

    if (strcmp(key, "ro.build.version.emui") == 0) {
        hooked_result = env->NewStringUTF("EmotionUI_8.0.0");
    } else if (strcmp(key, "ro.build.hw_emui_api_level") == 0) {
        hooked_result = env->NewStringUTF("21");
    }

    env->ReleaseStringUTFChars(keyJ, key);
    env->ReleaseStringUTFChars(defJ, def);

    if (hooked_result != nullptr) {
        return hooked_result;
    } else {
        return orig_native_get(env, clazz, keyJ, defJ);
    }
}

class HmsPushZygisk : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        const char *nice_name = env->GetStringUTFChars(args->nice_name, nullptr);
        const char *app_data_dir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (nice_name == nullptr || app_data_dir == nullptr) {
            return;
        }

        char *package_name = parsePackageName(app_data_dir);

        LOGD("packageName = [%s], process = [%s]\n", package_name, nice_name);

        string process(nice_name);
        string packageName(package_name);

        preSpecialize(package_name, process);

        env->ReleaseStringUTFChars(args->nice_name, nice_name);
        env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
        delete package_name;
    }

private:
    Api *api;
    JNIEnv *env;

    static char *parsePackageName(const char *app_data_dir) {
        if (*app_data_dir) {
            char *package_name = new char[256];
            // /data/user/<user_id>/<package>
            if (sscanf(app_data_dir, "/data/%*[^/]/%*[^/]/%s", package_name) == 1) {
                return package_name;
            }

            // /mnt/expand/<id>/user/<user_id>/<package>
            if (sscanf(app_data_dir, "/mnt/expand/%*[^/]/%*[^/]/%*[^/]/%s", package_name) == 1) {
                return package_name;
            }

            // /data/data/<package>
            if (sscanf(app_data_dir, "/data/%*[^/]/%s", package_name) == 1) {
                return package_name;
            }
        }
        return nullptr;
    }

    void hookSystemProperties() {
        LOGD("hook SystemProperties\n");

        JNINativeMethod targetHookMethods[] = {
                {"native_get", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                 (void *) my_native_get},
        };

        api->hookJniNativeMethods(env, "android/os/SystemProperties", targetHookMethods, 1);

        *(void **) &orig_native_get = targetHookMethods[0].fnPtr;

        LOGD("hook SystemProperties done: %p\n", orig_native_get);
    }

    void hookBuild() {
        LOGD("hook Build\n");
        jclass build_class = env->FindClass("android/os/Build");
        jstring new_brand = env->NewStringUTF("Huawei");
        jstring new_manufacturer = env->NewStringUTF("HUAWEI");

        jfieldID brand_id = env->GetStaticFieldID(build_class, "BRAND", "Ljava/lang/String;");
        if (brand_id != nullptr) {
            env->SetStaticObjectField(build_class, brand_id, new_brand);
        }

        jfieldID manufacturer_id = env->GetStaticFieldID(build_class, "MANUFACTURER", "Ljava/lang/String;");
        if (manufacturer_id != nullptr) {
            env->SetStaticObjectField(build_class, manufacturer_id, new_manufacturer);
        }

        env->DeleteLocalRef(new_brand);
        env->DeleteLocalRef(new_manufacturer);

        LOGD("hook Build done");
    }

    void preSpecialize(const string &packageName, const string &process) {
        auto list = requestRemoteConfig(packageName);

        bool shouldHook = false;
        for (const auto &item: list) {
            shouldHook = item.empty() || item == process;
            if (shouldHook)break;
        }

        if (shouldHook) {
            LOGI("hook package = [%s], process = [%s]\n", packageName.c_str(), process.c_str());
            hookBuild();
            hookSystemProperties();
            return;
        }

        // Since we do not hook any functions, we should let Zygisk dlclose ourselves
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

    /**
     * Request remote config from companion
     * @param packageName
     * @return list of processes to hook
     */
    vector<string> requestRemoteConfig(const string &packageName) {
        auto fd = api->connectCompanion();

        vector<char> content;

        auto size = receiveFile(fd, content);
        auto configs = parseConfig(content, packageName);

        LOGD("Loaded module payload: %d bytes", size);

        close(fd);

        return configs;
    }

    static int receiveFile(int remote_fd, vector<char> &buf) {
        off_t size;
        int ret = read(remote_fd, &size, sizeof(size));
        if (ret < 0) {
            LOGE("Failed to read size");
            return -1;
        }

        buf.resize(size);

        int bytesReceived = 0;
        while (bytesReceived < size) {
            ret = read(remote_fd, buf.data() + bytesReceived, size - bytesReceived);
            if (ret < 0) {
                LOGE("Failed to read data");
                return -1;
            }
            bytesReceived += ret;
        }
        return bytesReceived;
    }

    static vector<string> parseConfig(const vector<char> &content, const string &packageName) {
        vector<string> result;
        string line;
        for (char c: content) {
            if (c == '\n') {
                if (!line.empty()) {
                    size_t delimiterPos = line.find('|');
                    bool found = delimiterPos != string::npos;
                    auto pkg = line.substr(0, found ? delimiterPos : line.size());
                    if (pkg == packageName) {
                        if (found) {
                            result.push_back(line.substr(delimiterPos + 1, line.size()));
                        } else {
                            result.push_back("");
                        }
                    }
                    line.clear();
                }
            } else {
                line.push_back(c);
            }
        }
        return result;
    }
};

// Register our module class and the companion handler function
REGISTER_ZYGISK_MODULE(HmsPushZygisk)

REGISTER_ZYGISK_COMPANION(Server::companion_handler)
