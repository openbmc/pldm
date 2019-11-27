sources = [
  'pldm_cmd_helper.cpp',
  'pldm_base_cmd.cpp',
  'pldm_platform_cmd.cpp',
  'pldm_bios_cmd.cpp',
  'pldmtool.cpp'
]

executable(
  'pldmtool',
  sources,
  implicit_include_directories: false,
  dependencies: [libpldm, libpldmutils],
  install: true,
  install_dir: get_option('bindir'))
