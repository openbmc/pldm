project('pldm', ['c', 'cpp'],
        version: '0.1', meson_version: '>=1.1.1',
        default_options: [
          'warning_level=3',
          'default_library=shared',
          'werror=true',
          'cpp_std=c++23',
          'buildtype=debugoptimized'
        ])

# Wno-psabi reduces the number of "Note:" messages when cross-compiling some STL
# stuff for ARM. See https://stackoverflow.com/questions/48149323/strange-gcc-warning-when-compiling-qt-project
# Basically, gcc 6 and gcc 7 are not ABI compatible, but since the whole OpenBMC
# project uses the same compiler, we can safely ignmore these info notes.
add_project_arguments('-Wno-psabi', language: 'cpp')


# Disable FORTIFY_SOURCE when compiling with no optimization
if(get_option('optimization') == '0')
  add_project_arguments('-U_FORTIFY_SOURCE',language:['cpp','c'])
  message('Disabling FORTIFY_SOURCE as optimization is set to 0')
endif

package_datadir = join_paths(get_option('prefix'), get_option('datadir'), meson.project_name())
package_localstatedir = join_paths(get_option('prefix'), get_option('localstatedir'), meson.project_name())

conf_data = configuration_data()
cpp = meson.get_compiler('cpp')

# Enable POSIX poll APIs in libpldm
if cpp.has_header('poll.h')
    conf_data.set('PLDM_HAS_POLL', 1)
endif
if get_option('libpldmresponder').allowed()
conf_data.set_quoted('BIOS_JSONS_DIR', join_paths(package_datadir, 'bios'))
conf_data.set_quoted('BIOS_TABLES_DIR', join_paths(package_localstatedir, 'bios'))
conf_data.set_quoted('PDR_JSONS_DIR', join_paths(package_datadir, 'pdr'))
conf_data.set_quoted('FRU_JSONS_DIR', join_paths(package_datadir, 'fru'))
conf_data.set_quoted('FRU_MASTER_JSON', join_paths(package_datadir, 'fru_master.json'))
conf_data.set_quoted('HOST_JSONS_DIR', join_paths(package_datadir, 'host'))
conf_data.set_quoted('EVENTS_JSONS_DIR', join_paths(package_datadir, 'events'))
conf_data.set('HEARTBEAT_TIMEOUT', get_option('heartbeat-timeout-seconds'))
conf_data.set('TERMINUS_ID', get_option('terminus-id'))
conf_data.set('TERMINUS_HANDLE',get_option('terminus-handle'))
conf_data.set('DBUS_TIMEOUT', get_option('dbus-timeout-value'))
add_project_arguments('-DLIBPLDMRESPONDER', language : ['c','cpp'])
endif
if get_option('softoff').allowed()
  conf_data.set('SOFTOFF_TIMEOUT_SECONDS', get_option('softoff-timeout-seconds'))
endif
if get_option('oem-ibm').allowed()
  conf_data.set_quoted('FILE_TABLE_JSON', join_paths(package_datadir, 'fileTable.json'))
  conf_data.set_quoted('LID_RUNNING_DIR', '/var/lib/phosphor-software-manager/hostfw/running')
  conf_data.set_quoted('LID_ALTERNATE_DIR', '/var/lib/phosphor-software-manager/hostfw/alternate')
  conf_data.set_quoted('LID_STAGING_DIR', '/var/lib/phosphor-software-manager/hostfw/staging')
  conf_data.set_quoted('LID_RUNNING_PATCH_DIR', '/usr/local/share/hostfw/running')
  conf_data.set_quoted('LID_ALTERNATE_PATCH_DIR', '/usr/local/share/hostfw/alternate')
  conf_data.set('DMA_MAXSIZE', get_option('oem-ibm-dma-maxsize'))
  add_project_arguments('-DOEM_IBM', language : ['c', 'cpp'])
endif
if get_option('oem-meta').allowed()
  add_project_arguments('-DOEM_META', language : ['c','cpp'])
endif
conf_data.set('NUMBER_OF_REQUEST_RETRIES', get_option('number-of-request-retries'))
conf_data.set('INSTANCE_ID_EXPIRATION_INTERVAL',get_option('instance-id-expiration-interval'))
conf_data.set('RESPONSE_TIME_OUT',get_option('response-time-out'))
conf_data.set('FLIGHT_RECORDER_MAX_ENTRIES',get_option('flightrecorder-max-entries'))
conf_data.set_quoted('HOST_EID_PATH', join_paths(package_datadir, 'host_eid'))
conf_data.set('MAXIMUM_TRANSFER_SIZE', get_option('maximum-transfer-size'))
if get_option('transport-implementation') == 'mctp-demux'
  conf_data.set('PLDM_TRANSPORT_WITH_MCTP_DEMUX', 1)
elif get_option('transport-implementation') == 'af-mctp'
  conf_data.set('PLDM_TRANSPORT_WITH_AF_MCTP', 1)
endif
conf_data.set_quoted('STATIC_EID_TABLE_PATH', join_paths(package_datadir, 'static_eid_table.json'))
conf_data.set('SENSOR_POLLING_TIME', get_option('sensor-polling-time'))
conf_data.set_quoted('PLDM_T2_CONFIG_JSON', join_paths(package_datadir, 'pldm_t2_config.json'))
conf_data.set('LOCAL_EID', get_option('local-eid'))
config = configure_file(output: 'config.h',
  configuration: conf_data
)

add_project_arguments('-include', '@0@'.format(config), language: 'cpp')

filesystem = import('fs')

phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
stdplus = dependency('stdplus')
phosphor_logging_dep = dependency('phosphor-logging')

nlohmann_json_dep = dependency('nlohmann_json', include_type: 'system')

if cpp.has_header('CLI/CLI.hpp')
  CLI11_dep = declare_dependency()
else
  CLI11_dep = dependency('CLI11')
endif

if get_option('oe-sdk').allowed()
  # Setup OE SYSROOT
  OECORE_TARGET_SYSROOT = run_command('sh', '-c', 'echo $OECORE_TARGET_SYSROOT').stdout().strip()
  if OECORE_TARGET_SYSROOT == ''
      error('Unable to get $OECORE_TARGET_SYSROOT, check your environment.')
  endif
  message('OE_SYSROOT: ' + OECORE_TARGET_SYSROOT)
  rpath = ':'.join([OECORE_TARGET_SYSROOT + '/lib', OECORE_TARGET_SYSROOT + '/usr/lib'])
  ld_so = run_command('sh', '-c', 'find ' + OECORE_TARGET_SYSROOT + '/lib/ld-*.so | sort -r -n | head -n1').stdout().strip()
  dynamic_linker = ['-Wl,-dynamic-linker,' + ld_so]
else
  dynamic_linker = []
endif

if get_option('tests').allowed()
    gtest = dependency('gtest', main: true, disabler: true, required: false)
    gmock = dependency('gmock', disabler: true, required: false)
    if not gtest.found() or not gmock.found()
        gtest_proj = import('cmake').subproject('googletest', required: false)
        if gtest_proj.found()
            gtest = declare_dependency(
                dependencies: [
                    dependency('threads'),
                    gtest_proj.dependency('gtest'),
                    gtest_proj.dependency('gtest_main'),
                ]
            )
            gmock = gtest_proj.dependency('gmock')
        else
            assert(
                not get_option('tests').allowed(),
                'Googletest is required if tests are enabled'
            )
        endif
    endif
endif

libpldm_dep = dependency('libpldm', fallback:['libpldm','libpldm_dep'], include_type: 'system')


libpldmutils_headers = ['.']
libpldmutils = library(
  'pldmutils',
  'common/transport.cpp',
  'common/utils.cpp',
  version: meson.project_version(),
  dependencies: [
      libpldm_dep,
      phosphor_dbus_interfaces,
      phosphor_logging_dep,
      nlohmann_json_dep,
      sdbusplus,
  ],
  install: true,
  include_directories: include_directories(libpldmutils_headers),
)

libpldmutils = declare_dependency(
  include_directories: include_directories(libpldmutils_headers),
  link_with: libpldmutils)

deps = [
  libpldm_dep,
  libpldmutils,
  nlohmann_json_dep,
  phosphor_dbus_interfaces,
  phosphor_logging_dep,
  sdbusplus,
  sdeventplus,
  stdplus,
]

if get_option('libpldmresponder').allowed()
subdir('libpldmresponder')
deps += [
  libpldmresponder_dep
]
endif

source_files = [
  'pldmd/pldmd.cpp',
  'pldmd/dbus_impl_pdr.cpp',
  'fw-update/inventory_manager.cpp',
  'fw-update/package_parser.cpp',
  'fw-update/device_updater.cpp',
  'fw-update/watch.cpp',
  'fw-update/update_manager.cpp',
  'platform-mc/terminus_manager.cpp',
  'platform-mc/terminus.cpp',
  'platform-mc/platform_manager.cpp',
  'platform-mc/sensor_manager.cpp',
  'platform-mc/numeric_sensor.cpp',
  'platform-mc/event_manager.cpp',
  'requester/mctp_endpoint_discovery.cpp',
  'requester/configuration_discovery_handler.cpp',
]

if get_option('oem-meta').enabled()
  source_files += ['oem/meta/platform-mc/event_oem_meta.cpp']
endif

executable(
  'pldmd',
  source_files,
  implicit_include_directories: false,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir')
)

if get_option('systemd').allowed()
  systemd_system_unit_dir = dependency('systemd').get_variable(
          'systemdsystemunitdir')
  filesystem.copyfile(
    'pldmd/service_files/pldmd.service',
    'pldmd.service',
    install: true,
    install_dir: systemd_system_unit_dir
  )

  if get_option('oem-ibm').allowed()
    subdir('oem/ibm/service_files')
  endif
endif

subdir('pldmtool')

subdir('configurations')

if get_option('utilities').allowed()
  subdir('utilities')
endif

if get_option('softoff').allowed()
  subdir('softoff')
endif

if get_option('tests').allowed()
  subdir('common/test')
  subdir('fw-update/test')
  subdir('host-bmc/test')
  subdir('requester/test')
  subdir('platform-mc/test')
  subdir('test')
endif
