project('pldm', ['c', 'cpp'],
        version: '0.1', meson_version: '>=0.49.0',
        default_options: [
          'warning_level=3',
          'default_library=shared',
          'werror=true',
          'cpp_std=c++17'
        ])

# Wno-psabi reduces the number of "Note:" messages when cross-compiling some STL
# stuff for ARM. See https://stackoverflow.com/questions/48149323/strange-gcc-warning-when-compiling-qt-project
# Basically, gcc 6 and gcc 7 are not ABI compatible, but since the whole OpenBMC
# project uses the same compiler, we can safely ignmore these info notes.
add_project_arguments('-Wno-psabi', language: 'cpp')

conf_data = configuration_data()
conf_data.set_quoted('BIOS_JSONS_DIR', '/usr/share/pldm/bios')
conf_data.set_quoted('BIOS_TABLES_DIR', '/var/lib/pldm/bios')
conf_data.set_quoted('PDR_JSONS_DIR', '/usr/share/pldm/pdr')
conf_data.set_quoted('FRU_JSONS_DIR', '/usr/share/pldm/fru')
if get_option('oem-ibm').enabled()
  conf_data.set_quoted('FILE_TABLE_JSON', '/usr/share/pldm/fileTable.json')
  conf_data.set_quoted('LID_PERM_DIR', '/usr/share/host-fw')
  conf_data.set_quoted('LID_TEMP_DIR', '/usr/share/host-fw')
  add_global_arguments('-DOEM_IBM', language : 'c')
  add_global_arguments('-DOEM_IBM', language : 'cpp')
endif
configure_file(output: 'config.h',
  configuration: conf_data
)

subdir('libpldm')
subdir('libpldmresponder')
subdir('tool')

deps = [
  libpldm,
  libpldmresponder,
  dependency('sdbusplus'),
  dependency('sdeventplus'),
  dependency('phosphor-dbus-interfaces')
]

executable(
  'pldmd',
  'pldmd.cpp',
  'dbus_impl_requester.cpp',
  'instance_id.cpp',
  implicit_include_directories: false,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'))

if get_option('tests').enabled()
  subdir('test')
endif

if get_option('utilities').enabled()
  subdir('utilities')
endif
