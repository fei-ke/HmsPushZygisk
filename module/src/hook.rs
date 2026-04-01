use std::sync::OnceLock;

use jni::{
    objects::{JClass, JObject, JString, JValue},
    strings::JNIStr,
    sys::JNINativeMethod,
    JNIEnv,
};
use log::debug;

use zygisk_api::api::{ZygiskApi, V4};

// Storage for the original `native_get` function pointer saved by Zygisk.
type NativeGetFn = unsafe extern "C" fn(
    *mut jni::sys::JNIEnv,
    jni::sys::jclass,
    jni::sys::jstring,
    jni::sys::jstring,
) -> jni::sys::jstring;

static ORIG_NATIVE_GET: OnceLock<NativeGetFn> = OnceLock::new();
static SPOOFED_SYS_PROPS: OnceLock<&'static [(&'static str, &'static str)]> = OnceLock::new();

/// Hook replacement for `SystemProperties.native_get`.
unsafe extern "C" fn my_native_get(
    env: *mut jni::sys::JNIEnv,
    clazz: jni::sys::jclass,
    key_j: jni::sys::jstring,
    def_j: jni::sys::jstring,
) -> jni::sys::jstring {
    // SAFETY: env is a valid JNI pointer provided by the Android runtime.
    let mut jni_env = unsafe { JNIEnv::from_raw(env).expect("invalid JNIEnv") };

    let key: String = if key_j.is_null() {
        String::new()
    } else {
        jni_env
            .get_string(unsafe { &JString::from_raw(key_j) })
            .map(Into::into)
            .unwrap_or_default()
    };

    let sys_props = SPOOFED_SYS_PROPS.get().cloned().unwrap_or(&[]);
    let spoofed: Option<&str> = sys_props
        .iter()
        .find(|(k, _)| *k == key.as_str())
        .map(|(_, v)| *v);

    if let Some(value) = spoofed {
        let result = jni_env
            .new_string(value)
            .expect("failed to create JNI string");
        result.into_raw()
    } else {
        match ORIG_NATIVE_GET.get() {
            Some(orig) => unsafe { orig(env, clazz, key_j, def_j) },
            None => def_j,
        }
    }
}

/// Set android.os.Build properties from config.
pub fn hook_build(env: &mut JNIEnv<'_>, props: &[(&str, &str)], skip_build: bool) {
    if skip_build {
        debug!("skip hook Build");
        return;
    }

    debug!("hook Build");

    let build_class = match env.find_class("android/os/Build") {
        Ok(c) => c,
        Err(e) => {
            debug!("find_class android/os/Build failed: {:?}", e);
            return;
        }
    };

    for (prop, value) in props {
        set_static_string_field(env, &build_class, prop, value);
    }

    debug!("hook Build done");
}

fn set_static_string_field(env: &mut JNIEnv<'_>, class: &JClass<'_>, field: &str, value: &str) {
    let sig = "Ljava/lang/String;";

    let field_id = match env.get_static_field_id(class, field, sig) {
        Ok(id) => id,
        Err(err) => {
            debug!("get_static_field_id {} failed: {:?}", field, err);
            return;
        }
    };
    let new_str = match env.new_string(value) {
        Ok(s) => s,
        Err(err) => {
            debug!("new_string {} failed: {:?}", value, err);
            return;
        }
    };
    let obj = JObject::from(new_str);
    if let Err(err) = env.set_static_field(class, field_id, JValue::Object(&obj)) {
        debug!("set_static_field {} failed: {:?}", field, err);
    }
}

/// Replace `SystemProperties.native_get` via Zygisk's `hookJniNativeMethods`.
pub fn hook_system_properties(
    api: &mut ZygiskApi<'_, V4>,
    env: JNIEnv<'_>,
    sys_props: &'static [(&'static str, &'static str)],
) {
    debug!("hook SystemProperties");

    let _ = SPOOFED_SYS_PROPS.set(sys_props);

    // JNIStr is the type required by hook_jni_native_methods (impl Deref<Target = JNIStr>)
    // SAFETY: literal is valid UTF-8 and contains no interior NUL.
    let class_name: &JNIStr = unsafe { JNIStr::from_ptr(c"android/os/SystemProperties".as_ptr()) };

    let method_name = c"native_get".as_ptr().cast_mut();
    let signature = c"(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"
        .as_ptr()
        .cast_mut();

    let mut methods = [JNINativeMethod {
        name: method_name,
        signature,
        fnPtr: my_native_get as *mut _,
    }];

    // SAFETY: class_name is nul-terminated, methods slice is valid for the duration of the call.
    unsafe {
        api.hook_jni_native_methods(env, class_name, methods.as_mut_slice());
    }

    // Zygisk writes the original fn pointer back into methods[0].fnPtr
    let orig_ptr = methods[0].fnPtr;
    if !orig_ptr.is_null() {
        let orig_fn: NativeGetFn = unsafe { std::mem::transmute(orig_ptr) };
        let _ = ORIG_NATIVE_GET.set(orig_fn);
        debug!("hook SystemProperties done: {:?}", orig_ptr);
    }
}
