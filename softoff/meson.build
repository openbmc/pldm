deps = [
    libpldm, 
    libpldmutils,
    dependency('sdeventplus'),
    dependency('sdbusplus'),
    dependency('sdeventplus'),
    dependency('phosphor-dbus-interfaces'),
    ]

source = ['main.cpp','softoff.cpp']

executable('pldm-softpoweroff',source,
           implicit_include_directories: false,
           dependencies: deps,
           install: true,
           install_dir: get_option('bindir'))

install_data(
    'guardChassisOffTarget.sh',
    install_mode: 'rwxr-xr-x',
    install_dir: get_option('bindir'),
)
