test_sources = [
    '../../common/utils.cpp',
    '../utils.cpp',
    '../dbus/custom_dbus.cpp',
    '../dbus/asset.cpp',
    '../dbus/availability.cpp',
    '../dbus/cable.cpp',
    '../dbus/chassis.cpp',
    '../dbus/cpu_core.cpp',
    '../dbus/inventory_item.cpp',
    '../dbus/pcie_device.cpp',
    '../dbus/pcie_slot.cpp',
]

host_bmc_test_deps = [
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

host_bmc_test_lib = static_library(
    'host_bmc_test_common',
    test_sources,
    implicit_include_directories: false,
    dependencies: host_bmc_test_deps,
)
host_bmc_test_deps += declare_dependency(link_whole: host_bmc_test_lib)

tests = ['utils_test', 'custom_dbus_test', 'dbus_to_event_handler_test']

foreach t : tests
    test_deps = host_bmc_test_deps
    if t == 'dbus_to_event_handler_test'
        test_deps += [libpldmresponder_dep]
    endif
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            dependencies: test_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
