deps = [
  dependency('phosphor-dbus-interfaces'),
  dependency('phosphor-logging'),
  dependency('sdbusplus'),
  libpldm
]

sources = [
  'base.cpp',
  'bios.cpp',
  'bios_table.cpp',
  'bios_parser.cpp',
  'pdr.cpp',
  'effecters.cpp',
  'utils.cpp',
  'platform.cpp',
  'fru_parser.cpp'
]

if get_option('oem-ibm').enabled()
  sources += [
    '../oem/ibm/libpldmresponder/file_io.cpp',
    '../oem/ibm/libpldmresponder/file_table.cpp',
    '../oem/ibm/libpldmresponder/file_io_by_type.cpp',
    '../oem/ibm/libpldmresponder/file_io_type_pel.cpp'
  ]
endif

libpldmresponder = library(
  'pldmresponder',
  sources,
  version: meson.project_version(),
  dependencies: deps,
  install: true)

libpldmresponder = declare_dependency(
  link_with: libpldmresponder)
