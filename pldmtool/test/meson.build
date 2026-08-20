pldmtool_test_src = declare_dependency(
    sources: ['../pldm_timestamp104.cpp'],
    include_directories: include_directories('..'),
)

tests = ['pldm_timestamp104_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            dependencies: [pldmtool_test_src, gtest, libpldm_dep],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
