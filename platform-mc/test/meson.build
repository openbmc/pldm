platform_mc_test_deps = [
    gtest,
    gmock,
    libpldm_dep,
    libpldmutils,
    nlohmann_json_dep,
    phosphor_dbus_interfaces,
    phosphor_logging_dep,
    sdbusplus,
    sdeventplus,
]

platform_mc_test_lib = static_library(
    'platform_mc_test_common',
    [
        '../terminus_manager.cpp',
        '../terminus.cpp',
        '../platform_manager.cpp',
        '../manager.cpp',
        '../sensor_manager.cpp',
        '../numeric_sensor.cpp',
        '../event_manager.cpp',
        '../dbus_to_terminus_effecters.cpp',
        '../entity.cpp',
        '../state_effecter.cpp',
        '../state_sensor.cpp',
        '../state_set.cpp',
        '../../requester/mctp_endpoint_discovery.cpp',
    ],
    implicit_include_directories: false,
    include_directories: ['../../requester', '../../pldmd'],
    dependencies: platform_mc_test_deps,
)
test_src = declare_dependency(
    include_directories: ['../../requester', '../../pldmd'],
    link_whole: platform_mc_test_lib,
)

tests = [
    'terminus_manager_test',
    'terminus_test',
    'platform_manager_test',
    'sensor_manager_test',
    'numeric_sensor_test',
    'event_manager_test',
    'dbus_to_terminus_effecter_test',
    'dbus_impl_fru_test',
    'state_set_test',
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            dependencies: platform_mc_test_deps + [test_src],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
