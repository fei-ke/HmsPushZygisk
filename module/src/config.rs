pub const CONFIG_PATH: &str = "/data/adb/hmspush/app.conf";

pub const HMSPUSH_PACKAGE_NAME: &str = "one.yufz.hmspush";

#[derive(Clone, Copy)]
pub struct PackageProps<'a> {
    pub package_name: &'a str,
    pub system_properties: &'a [(&'a str, &'a str)],
    pub build_properties: &'a [(&'a str, &'a str)],
}

pub const DEFAULT_PACKAGE_PROPS: PackageProps<'static> = PackageProps {
    package_name: "",
    system_properties: &[
        ("ro.build.version.emui", "EmotionUI_8.0.0"),
        ("ro.build.hw_emui_api_level", "21"),
    ],
    build_properties: &[("BRAND", "Huawei"), ("MANUFACTURER", "HUAWEI")],
};

pub const PACKAGE_PROPS: &[PackageProps] = &[
    PackageProps {
        package_name: HMSPUSH_PACKAGE_NAME,
        system_properties: &[("hmspush.zygisk.enabled", "true")],
        build_properties: &[],
    },
    PackageProps {
        package_name: "com.sankuai.meituan",
        system_properties: &[("ro.build.version.emui", "EmotionUI_8.0.0")],
        build_properties: &[],
    },
    PackageProps {
        package_name: "com.sankuai.meituan.takeoutnew",
        system_properties: &[("ro.build.version.emui", "EmotionUI_8.0.0")],
        build_properties: &[],
    },
    PackageProps {
        package_name: "com.dianping.v1",
        system_properties: &[("ro.build.version.emui", "EmotionUI_8.0.0")],
        build_properties: &[],
    },
];

#[inline]
pub fn get_properties_for_package(pkg: &str) -> PackageProps<'static> {
    PACKAGE_PROPS
        .iter()
        .find(|p| p.package_name == pkg)
        .copied()
        .unwrap_or(DEFAULT_PACKAGE_PROPS)
}
