test_src = declare_dependency(
          sources: [
            '../terminus_manager.cpp',
            '../terminus.cpp',
            '../platform_manager.cpp',
            '../sensor_manager.cpp',
            '../numeric_sensor.cpp',
            '../../requester/mctp_endpoint_discovery.cpp',
            '../../pldmd/dbus_impl_requester.cpp',
            '../../pldmd/instance_id.cpp'],
            include_directories: '../../requester')

tests = [
  'terminus_manager_test',
  'terminus_test',
  'sensor_manager_test',
  'numeric_sensor_test',
]

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     link_args: dynamic_linker,
                     build_rpath: get_option('oe-sdk').enabled() ? rpath : '',
                     dependencies: [
                         gtest,
                         gmock,
                         libpldm_dep,
                         libpldmutils,
                         nlohmann_json,
                         phosphor_dbus_interfaces,
                         sdbusplus,
                         sdeventplus,
                         test_src]),
       workdir: meson.current_source_dir())
endforeach
