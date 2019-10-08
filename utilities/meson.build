deps = [
  libpldm,
]

executable(
  'set-state-effecter',
  'requester/set_state_effecter.cpp',
  implicit_include_directories: false,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'))
