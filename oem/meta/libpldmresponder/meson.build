oem_files += files(
    'file_io.cpp',
    'file_io_type_bios_version.cpp',
    'file_io_type_event_log.cpp',
    'file_io_type_http_boot.cpp',
    'file_io_type_post_code.cpp',
    'file_io_type_power_control.cpp',
)

if get_option('tests').allowed()
    test(
        'http_boot_file_io_test',
        executable(
            'http_boot_file_io_test',
            'file_io_type_http_boot_test.cpp',
            'file_io_type_http_boot.cpp',
            dependencies: [gtest, libpldm_dep, phosphor_logging_dep],
        ),
    )
endif
