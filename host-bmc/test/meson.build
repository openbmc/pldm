host_bmc_test_src = declare_dependency(
          sources: [ '../dbus_to_host_effecters.cpp' ],
          include_directories: '../../requester')

test_sources = [
  '../../common/utils.cpp',
  '../custom_dbus.cpp',
]

tests = [
  'dbus_to_host_effecter_test',
  'utils_test',
  'custom_dbus_test',
]

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     test_sources,
                     implicit_include_directories: false,
                     dependencies: [
                         gtest,
                         gmock,
                         host_bmc_test_src,
                         libpldm_dep,
                         libpldmutils,
                         nlohmann_json_dep,
                         phosphor_dbus_interfaces,
                         phosphor_logging_dep,
                         sdbusplus,
                         sdeventplus]),
       workdir: meson.current_source_dir())
endforeach
