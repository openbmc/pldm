oem_files += files('cper.cpp', 'oem_event_manager.cpp')

libcper_dep = dependency('libcper', include_type: 'system')
deps += [libcper_dep]

if get_option('tests').allowed()
    test(
        'cper_test',
        executable(
            'cper_test',
            'cper_test.cpp',
            'cper.cpp',
            dependencies: [
                gtest,
                libcper_dep,
                libpldm_dep,
                libpldmutils,
                phosphor_logging_dep,
                sdbusplus,
            ],
        ),
    )
endif
