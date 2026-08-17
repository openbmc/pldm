oem_files += files('cper.cpp', 'oem_event_manager.cpp')

libcper_dep = dependency('libcper', include_type: 'system')
deps += [libcper_dep]

if get_option('tests').allowed()
    subdir('test')
endif
