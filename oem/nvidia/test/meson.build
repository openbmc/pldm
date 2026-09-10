test_src = declare_dependency(sources: ['../inventory_event_handler.cpp'])

tests = ['inventory_event_handler_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            dependencies: [
                gtest,
                gmock,
                libpldm_dep,
                libpldmutils,
                nlohmann_json_dep,
                phosphor_dbus_interfaces,
                phosphor_logging_dep,
                sdbusplus,
                test_src,
            ],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
