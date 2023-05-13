#ifndef HMSPUSHZYGISK_HOOK_H
#define HMSPUSHZYGISK_HOOK_H


#include <jni.h>
#include <fcntl.h>
#include "zygisk.hpp"

using zygisk::Api;

class Hook {
public:
    Hook(Api *api, JNIEnv *env) {
        this->api = api;
        this->env = env;
    }

    void hook();

private:
    Api *api;
    JNIEnv *env;
};

#endif //HMSPUSHZYGISK_HOOK_H