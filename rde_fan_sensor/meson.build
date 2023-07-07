sources = [
  'RdePwmSensor.cpp',
  'RdeTachSensor.cpp',
  'RdeUtils.cpp',
  'RdeFanSensorMain.cpp'
]
headers = ['.']
requester_inc = include_directories(headers)
req_src = declare_dependency(
          include_directories:requester_inc)

boost = dependency('boost',version : '>=1.80.0', required : false, include_type: 'system')
if not boost.found()
  boost = subproject('boost', required: true).get_variable('boost_dep')
  boost = boost.as_system('system')
endif

deps = [
  libpldm_dep,
  libbej_dep,
  boost,
  nlohmann_json,
  phosphor_dbus_interfaces,
  libpldmutils,
  sdbusplus,
  sdeventplus,
  stdplus,
  req_src,
]

rde_fan_lib_static = static_library('runtime',
                                    sources,
                                    install : true)

executable(
  'rde_fan_sensor',
  link_with: rde_fan_lib_static,
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
  input: 'service_files/rde_fan_sensor.service',
  install: true,
  install_dir: systemd_system_unit_dir,
  output: 'rde_fan_sensor.service',
)
endif

