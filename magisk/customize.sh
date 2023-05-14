#Some apps will get stuck when getting 'oaid',
#But on non-huawei devices, we don't have one, so we provider a default one.
#You can delete these by execute the following command:
#adb shell settings delete global pps_oaid
#adb shell settings delete global pps_track_limit
settings put global pps_oaid 00000000-0000-0000-0000-000000000000
settings put global pps_track_limit true