libpldm_headers = ['.', './oem/ibm']

sources = [
    'pldm_cmd_helper.cpp',
    'pldm_base_cmd.cpp',
    'pldm_platform_cmd.cpp',
    'pldm_bios_cmd.cpp',
    'pldm_fru_cmd.cpp',
    'pldm_fw_update_cmd.cpp',
    'pldmtool.cpp',
]

if get_option('oem-ibm').allowed()
    sources += ['oem/ibm/pldm_oem_ibm.cpp']
endif

executable(
    'pldmtool',
    sources,
    implicit_include_directories: false,
    include_directories: include_directories(libpldm_headers),
    dependencies: [
        CLI11_dep,
        libpldm_dep,
        libpldmutils,
        nlohmann_json_dep,
        phosphor_dbus_interfaces,
        phosphor_logging_dep,
        sdbusplus,
    ],
    install: true,
    install_dir: get_option('bindir'),
)

if get_option('tests').allowed()
    test_pldmtool = executable(
        'pldmtool-test',
        sources,
        '../common/utils.cpp',
        'test/transport.cpp',
        implicit_include_directories: false,
        include_directories: include_directories('..', libpldm_headers),
        dependencies: [
            CLI11_dep,
            libpldm_dep,
            nlohmann_json_dep,
            phosphor_dbus_interfaces,
            phosphor_logging_dep,
            sdbusplus,
        ],
        link_args: [
            '-Wl,--wrap=pldm_instance_db_init_default',
            '-Wl,--wrap=pldm_instance_id_alloc',
            '-Wl,--wrap=pldm_instance_id_free',
            '-Wl,--wrap=pldm_instance_db_destroy',
        ],
    )
    test(
        'pldmtool-state-effecter-enables',
        find_program('python3'),
        args: [files('test/state-effecter-enables.py'), test_pldmtool],
        timeout: 120,
    )
endif
