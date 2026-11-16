requester_test_deps = [
    gtest,
    gmock,
    libpldm_dep,
    nlohmann_json_dep,
    phosphor_dbus_interfaces,
    phosphor_logging_dep,
    sdbusplus,
    sdeventplus,
]

requester_test_lib = static_library(
    'requester_test_common',
    ['../mctp_endpoint_discovery.cpp', '../../common/utils.cpp'],
    implicit_include_directories: false,
    include_directories: ['../../', '../../pldmd'],
    dependencies: requester_test_deps,
)
test_src = declare_dependency(link_whole: requester_test_lib)

tests = ['handler_test', 'request_test', 'mctp_endpoint_discovery_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            include_directories: ['../../', '../../pldmd'],
            dependencies: requester_test_deps + [test_src],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
