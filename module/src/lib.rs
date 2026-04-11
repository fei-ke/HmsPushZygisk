use android_logger::Config;
use jni::JNIEnv;
use log::{debug, error, info, LevelFilter};
use std::io::{Read, Write};
use std::os::unix::net::UnixStream;
use zygisk_api::{
    api::{ZygiskApi, V4},
    raw::ZygiskRaw,
    register_companion, register_module, ZygiskModule,
};
// ZygiskOption is re-exported by the V4 transparent module via `pub use crate::raw::v4::transparent::*`
use zygisk_api::api::v4::ZygiskOption;

pub mod config;
use config::HMSPUSH_PACKAGE_NAME;
mod hook;
mod server;

#[derive(Default)]
struct HmsPushModule;

impl ZygiskModule for HmsPushModule {
    type Api = V4;

    fn pre_app_specialize<'a>(
        &self,
        mut api: ZygiskApi<'a, V4>,
        mut env: JNIEnv<'a>,
        args: &'a mut <V4 as ZygiskRaw<'_>>::AppSpecializeArgs,
    ) {
        android_logger::init_once(
            Config::default()
                .with_max_level(LevelFilter::Debug)
                .with_tag("HmsPushZygisk"),
        );

        // args.nice_name and args.app_data_dir are &JString<'a>
        let process_name = jstring_to_string(&mut env, args.nice_name);
        let app_data_dir = jstring_to_string(&mut env, args.app_data_dir);

        if process_name.is_empty() || app_data_dir.is_empty() {
            api.set_option(ZygiskOption::DlCloseModuleLibrary);
            return;
        }

        let package_name = parse_package_name(&app_data_dir);
        debug!(
            "preAppSpecialize, packageName = {}, process = {}",
            package_name, process_name
        );

        pre_specialize(api, env, package_name, &process_name);
    }

    fn pre_server_specialize<'a>(
        &self,
        mut api: ZygiskApi<'a, V4>,
        _env: JNIEnv<'a>,
        _args: &'a mut <V4 as ZygiskRaw<'_>>::ServerSpecializeArgs,
    ) {
        api.set_option(ZygiskOption::DlCloseModuleLibrary);
    }
}

/// Convert a JString reference to a Rust String.
fn jstring_to_string(env: &mut JNIEnv<'_>, jstr: &jni::objects::JString<'_>) -> String {
    // SAFETY: env comes from Zygisk's trusted JNI entry which provides a valid env.
    match env.get_string(jstr) {
        Ok(s) => s.into(),
        Err(_) => String::new(),
    }
}

/// Extract the last path component (package name) from an app_data_dir path.
/// Handles: /data/user/<uid>/<pkg>, /data/data/<pkg>, /mnt/expand/.../<pkg>
fn parse_package_name(app_data_dir: &str) -> &str {
    app_data_dir
        .rsplit('/')
        .find(|s| !s.is_empty())
        .unwrap_or("")
}

fn pre_specialize(
    mut api: ZygiskApi<'_, V4>,
    mut env: JNIEnv<'_>,
    package_name: &str,
    process: &str,
) {
    let should_hook = package_name == HMSPUSH_PACKAGE_NAME
        || query_should_hook(&mut api, package_name, process);

    if should_hook {
        info!("hook package = [{}], process = [{}]", package_name, process);

        let pkg_props = config::get_properties_for_package(package_name);

        if !pkg_props.build_properties.is_empty() {
            hook::hook_build(&mut env, pkg_props.build_properties);
        }
        
        if !pkg_props.system_properties.is_empty() {
            hook::hook_system_properties(&mut api, env, pkg_props.system_properties);
        }
    } else {
        api.set_option(ZygiskOption::DlCloseModuleLibrary);
    }
}

/// Ask the companion process whether this (package, process) pair should be hooked.
fn query_should_hook(api: &mut ZygiskApi<'_, V4>, package_name: &str, process_name: &str) -> bool {
    debug!(
        "query_should_hook: package = [{}], process = [{}]",
        package_name, process_name
    );

    let result = api.with_companion(|stream| send_query(stream, package_name, process_name));

    match result {
        Ok(should_hook) => should_hook,
        Err(e) => {
            error!("Failed to connect to companion: {:?}", e);
            false
        }
    }
}

/// Write "package_name\nprocess_name\n" to the companion and read back 1 byte.
fn send_query(stream: &mut UnixStream, package_name: &str, process_name: &str) -> bool {
    // Send the two fields as newline-terminated strings.
    let payload = format!("{}\n{}\n", package_name, process_name);
    if let Err(e) = stream.write_all(payload.as_bytes()) {
        error!("Failed to send query: {}", e);
        return false;
    }

    // Read the single-byte response: 1 = hook, 0 = skip.
    let mut resp = [0u8; 1];
    match stream.read_exact(&mut resp) {
        Ok(_) => resp[0] != 0,
        Err(e) => {
            error!("Failed to read companion response: {}", e);
            false
        }
    }
}

register_module!(HmsPushModule);
register_companion!(server::companion_handler);
