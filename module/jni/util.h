#ifndef HMSPUSHZYGISK_UTIL_H
#define HMSPUSHZYGISK_UTIL_H

#include <jni.h>

static std::string jstringToStdString(JNIEnv *env, jstring &jstr) {
    if (env == nullptr || jstr == nullptr || !jstr) {
        return "";
    }

    const char *chars = env->GetStringUTFChars(jstr, nullptr);
    if (chars && *chars) {
        std::string ret(chars);
        env->ReleaseStringUTFChars(jstr, chars);
        return ret;
    }
    return "";
}

#endif //HMSPUSHZYGISK_UTIL_H
