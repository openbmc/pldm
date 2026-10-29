oem_files += files(
    'crashlog.cpp',
    'oem_event_manager.cpp',
)

if get_option('tests').allowed()
    test(
        'arm_crashlog_storage_test',
        executable('arm_crashlog_storage_test', 'crashlog_storage_test.cpp'),
    )
endif
