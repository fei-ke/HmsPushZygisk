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
#include <string>
#include <fcntl.h>

#include "zygisk.hpp"
#include "logging.h"
#include "server.h"
#include "hook.h"
#include "util.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

using namespace std;

class HmsPushZygisk : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        string process_name = jstringToStdString(env, args->nice_name);
        string app_data_dir = jstringToStdString(env, args->app_data_dir);

        if (process_name.empty() || app_data_dir.empty()) {
            // Since we do not hook any functions, we should let Zygisk dlclose ourselves
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        string package_name = parsePackageName(app_data_dir.c_str());

        LOGD("preAppSpecialize, packageName = %s, process = %s\n", package_name.c_str(), process_name.c_str());

        preSpecialize(package_name, process_name);
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        // Never tamper with system_server
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;

    static string parsePackageName(const char *app_data_dir) {
        if (app_data_dir && *app_data_dir) {
            char package_name[256] = {0};
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
        return "";
    }

    void preSpecialize(const string &packageName, const string &process) {
        vector<string> processList = requestRemoteConfig(packageName);
        if (!processList.empty()) {
            bool shouldHook = false;
            for (const auto &item: processList) {
                if (item.empty() || item == process) {
                    shouldHook = true;
                    break;
                }
            }

            if (shouldHook) {
                LOGI("hook package = [%s], process = [%s]\n", packageName.c_str(), process.c_str());
                Hook(api, env).hook();
                return;
            }
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
        LOGD("requestRemoteConfig for %s", packageName.c_str());
        auto fd = api->connectCompanion();
        LOGD("connect to companion fd = %d", fd);
        vector<char> content;

        auto size = receiveConfig(fd, content);
        auto configs = parseConfig(content, packageName);
        LOGD("Loaded module payload: %d bytes, config size:%lu ", size, configs.size());

        close(fd);

        return configs;
    }

    static int receiveConfig(int remote_fd, vector<char> &buf) {
        LOGD("start receiving config");

        off_t size;
        int ret = read(remote_fd, &size, sizeof(size));
        if (ret == 0) {
            LOGD("receive empty config");
            return 0;
        }
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

        // Ensure the last byte is '\n'
        if (!buf.empty() && buf[buf.size() - 1] != '\n') {
            buf.push_back('\n');
        }
        return bytesReceived;
    }

    static vector<string> parseConfig(const vector<char> &content, const string &packageName) {
        vector<string> result;

        if (content.empty()) return result;

        string line;
        for (char c: content) {
            if (c == '\n') {
                if (!line.empty() || line[0] != '#') {
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
                }
                line.clear();
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
