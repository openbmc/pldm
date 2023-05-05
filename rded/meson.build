sources = [
  'rded.cpp',
]

args = [
  '-DBOOST_ASIO_DISABLE_THREADS',
  '-DBOOST_ALL_NO_LIB',
  '-DBOOST_SYSTEM_NO_DEPRECATED',
  '-DBOOST_ERROR_CODE_HEADER_ONLY',
  '-DBOOST_COROUTINES_NO_DEPRECATION_WARNING']
headers = ['.', '..']
requester_inc = include_directories(headers)
req_src = declare_dependency(
          sources: [
            '../pldmd/dbus_impl_requester.cpp',
            '../pldmd/instance_id.cpp'],
          include_directories:requester_inc)
boost = dependency('boost',version : '>=1.80.0', required : false, include_type: 'system')

if not boost.found()
  boost = subproject('boost', required: true).get_variable('boost_dep')
  boost = boost.as_system('system')
endif

boost_compile_args = [
    '-DBOOST_ASIO_DISABLE_THREADS',
    '-DBOOST_ALL_NO_LIB',
    '-DBOOST_SYSTEM_NO_DEPRECATED',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DBOOST_COROUTINES_NO_DEPRECATION_WARNING',
]

boost_dep = declare_dependency(
    dependencies: dependency('boost',modules: ['coroutine', 'context'], disabler:true, required: false),
    compile_args: boost_compile_args)

deps = [
  libpldm_dep,
  libbej_dep,
  boost_dep,
  nlohmann_json,
  phosphor_dbus_interfaces,
  libpldmutils,
  sdbusplus,
  sdeventplus,
  stdplus,
  req_src,
]
executable(
  'rded',
  sources,
  link_args: dynamic_linker,
  implicit_include_directories: false,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'),
  link_args : ['-ludev'])
if get_option('systemd').enabled()
  systemd_system_unit_dir = dependency('systemd').get_variable(
          pkgconfig: 'systemdsystemunitdir')
systemd_system_unit_dir = dependency('systemd').get_variable(
          pkgconfig: 'systemdsystemunitdir')
 
configure_file(
  copy: true,
  input: 'service_files/rded.service',
  install: true,
  install_dir: systemd_system_unit_dir,
  output: 'rded.service',
)
endif