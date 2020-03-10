deps = [ libpldm ]

source = ['main.cpp']

executable('pldm-softpoweroff',source,
           implicit_include_directories: false,
           dependencies: deps,
           install: true,
           install_dir: get_option('bindir'))
