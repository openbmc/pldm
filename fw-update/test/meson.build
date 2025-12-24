fw_update_test_src = declare_dependency(
    sources: [
        '../activation.cpp',
        '../aggregate_update_manager.cpp',
        '../inventory_manager.cpp',
        '../package_parser.cpp',
        '../device_updater.cpp',
        '../update_manager.cpp',
        '../firmware_inventory_manager.cpp',
        '../firmware_inventory.cpp',
        '../update.cpp',
        '../../common/utils.cpp',
    ],
)

tests = [
    'inventory_manager_test',
    'package_parser_test',
    'device_updater_test',
    'firmware_inventory_manager_test',
    'firmware_inventory_test',
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            include_directories: '../../pldmd',
            dependencies: [
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
                stdexec_dep,
            ],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
