sources = [
  'pldmmctpd.cpp',
  'pldm_base_requester.c',
  'pldm_base_helper.cpp',
]
headers = ['.', '..']
requester_inc = include_directories(headers)
req_src = declare_dependency(
          sources: [
            '../pldmd/dbus_impl_requester.cpp',
            '../pldmd/instance_id.cpp'],
          include_directories:requester_inc)

deps = [
  libpldm_dep,
  nlohmann_json,
  phosphor_dbus_interfaces,
  libpldmutils,
  sdbusplus,
  sdeventplus,
  stdplus,
  req_src,
]

executable(
  'pldmmctpd',
  sources,
  link_args: dynamic_linker,
  implicit_include_directories: false,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'))


if get_option('systemd').enabled()
  systemd_system_unit_dir = dependency('systemd').get_variable(
          pkgconfig: 'systemdsystemunitdir')

systemd_system_unit_dir = dependency('systemd').get_variable(
          pkgconfig: 'systemdsystemunitdir')
configure_file(
  copy: true,
  input: 'service_files/pldmmctpd.service',
  install: true,
  install_dir: systemd_system_unit_dir,
  output: 'pldmmctpd.service',
)
endif