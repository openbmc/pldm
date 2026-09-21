fw_update_test_src = declare_dependency(sources: fw_update_sources)

test_deps = [
    fw_update_test_src,
    gmock,
    gtest,
    libpldm_dep,
    libpldmutils,
    nlohmann_json_dep,
    phosphor_dbus_interfaces,
    phosphor_logging_dep,
    sdbusplus,
    sdeventplus,
]

tests = [
    'inventory_manager_test',
    'package_parser_test',
    'device_updater_test',
    'firmware_inventory_manager_test',
    'firmware_inventory_test',
    'condition_collector_test',
    'systemd_interface_test',
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            include_directories: '../../pldmd',
            dependencies: test_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach

# Claims the xyz.openbmc_project.Logging name to stand in for the logging
# service, so it cannot run alongside a test that commits events of its own.
test(
    'events_test',
    executable(
        'events_test',
        'events_test.cpp',
        implicit_include_directories: false,
        include_directories: '../../pldmd',
        dependencies: test_deps,
    ),
    workdir: meson.current_source_dir(),
    is_parallel: false,
)
