pldmd_inc = include_directories('../', '../pldmtool', '../pldmtool/oem/ibm')
test_src = declare_dependency(include_directories: pldmd_inc)

tests = ['pldmd_registration_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            dependencies: [
                gtest,
                libpldm_dep,
                nlohmann_json_dep,
                phosphor_logging_dep,
                test_src,
            ],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach

test(
    'pldmtool_platform_pdr_test',
    executable(
        'pldmtool_platform_pdr_test',
        'pldmtool_platform_pdr_test.cpp',
        '../pldmtool/pldm_platform_cmd.cpp',
        implicit_include_directories: false,
        dependencies: [
            CLI11_dep,
            gtest,
            libpldm_dep,
            libpldmutils,
            nlohmann_json_dep,
            phosphor_logging_dep,
            test_src,
        ],
    ),
    workdir: meson.current_source_dir(),
)
