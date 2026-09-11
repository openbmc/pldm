tests = ['pldm_cmd_helper_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            dependencies: [
                CLI11_dep,
                gtest,
                libpldm_dep,
                libpldmutils,
                nlohmann_json_dep,
                phosphor_dbus_interfaces,
                phosphor_logging_dep,
                sdbusplus,
            ],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
