pldmd_inc = include_directories('../')
test_src = declare_dependency(include_directories: pldmd_inc)

tests = ['pldmd_registration_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            dependencies: [libpldm_dep, nlohmann_json_dep, gtest, test_src],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
