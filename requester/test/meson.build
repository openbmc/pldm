test_src = declare_dependency(
          sources: [
            '../mctp_endpoint_discovery.cpp',
            '../configuration_discovery_handler.cpp'
          ])

tests = [
  'handler_test',
  'request_test',
  'mctp_endpoint_discovery_test',
  'configuration_discovery_handler_test',
]

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     include_directories: [ '../../', '../../pldmd' ],
                     link_args: dynamic_linker,
                     build_rpath: get_option('oe-sdk').allowed() ? rpath : '',
                     dependencies: [
                         gtest,
                         gmock,
                         libpldm_dep,
                         nlohmann_json_dep,
                         libpldmutils,
                         phosphor_dbus_interfaces,
                         phosphor_logging_dep,
                         sdbusplus,
                         sdeventplus,
                         test_src,
                    ]),
       workdir: meson.current_source_dir())
endforeach
