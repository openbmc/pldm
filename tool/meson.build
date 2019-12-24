sources = [
  'pldm_cmd_helper.cpp',
  'pldm_base_cmd.cpp',
  'pldmtool.cpp'
]

executable(
  'pldmtool',
  sources,
  implicit_include_directories: false,
  dependencies: [libpldm, libutility],
  install: true,
  install_dir: get_option('bindir'))
