deps = [
    libpldm,
    dependency('sdeventplus'),
    dependency('sdbusplus'),
    dependency('phosphor-dbus-interfaces'),
    ]

source = ['main.cpp','softoff.cpp']

executable('pldm-softpoweroff',source,
           implicit_include_directories: false,
           dependencies: deps,
           install: true,
           install_dir: get_option('bindir'))

