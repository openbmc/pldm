tests = [
  'handler_test',
  'request_test',
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
                         phosphor_dbus_interfaces,
                         phosphor_logging_dep,
                         sdbusplus,
                         sdeventplus,
                    ]),
       workdir: meson.current_source_dir())
endforeach
