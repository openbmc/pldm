test_src = declare_dependency(
          sources: [
            '../terminus_manager.cpp',
            '../terminus.cpp',
            '../platform_manager.cpp',
            '../manager.cpp',
            '../../requester/mctp_endpoint_discovery.cpp'],
            include_directories: ['../../requester', '../../pldmd'])

tests = [
  'terminus_manager_test',
  'terminus_test',
]

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     dependencies: [
                         gtest,
                         gmock,
                         libpldm_dep,
                         libpldmutils,
                         nlohmann_json_dep,
                         phosphor_dbus_interfaces,
                         phosphor_logging_dep,
                         sdbusplus,
                         sdeventplus,
                         test_src]),
       workdir: meson.current_source_dir())
endforeach
