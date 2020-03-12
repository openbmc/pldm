libpldm_headers = ['.', './oem/ibm']

sources = [
  'pldm_cmd_helper.cpp',
  'pldm_base_cmd.cpp',
  'pldm_platform_cmd.cpp',
  'pldm_bios_cmd.cpp',
  'pldm_fru_cmd.cpp',
  'pldmtool.cpp',
  'oem/ibm/pldm_power_host_cmd.cpp'
]

executable(
  'pldmtool',
  sources,
  implicit_include_directories: false,
  include_directories: include_directories(libpldm_headers),
  dependencies: [libpldm, libpldmutils, dependency('sdbusplus')],
  install: true,
  install_dir: get_option('bindir'))
