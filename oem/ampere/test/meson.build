test(
    'cper_test',
    executable(
        'cper_test',
        'cper_test.cpp',
        '../event/cper.cpp',
        implicit_include_directories: false,
        include_directories: include_directories('../../..', '../event'),
        dependencies: [deps, libcper_dep, gtest],
    ),
    workdir: meson.current_source_dir(),
)
