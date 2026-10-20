test(
    'oem_event_manager_test',
    executable(
        'oem_event_manager_test',
        'oem_event_manager_test.cpp',
        '../oem_event_manager.cpp',
        '../../utils.cpp',
        implicit_include_directories: false,
        dependencies: [gtest] + deps,
    ),
    workdir: meson.current_source_dir(),
)
