common_test_src = declare_dependency(
          sources: [
            '../utils.cpp'])

tests = [
  'pldm_utils_test',
]

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     link_args: dynamic_linker,
                     build_rpath: get_option('oe-sdk').enabled() ? rpath : '',
                     dependencies: [
                         gtest,
                         common_test_src,
                         libpldm_dep,
                         phosphor_dbus_interfaces,
                         phosphor_logging_dep,
                         nlohmann_json,
                         libpldmutils,
                         sdbusplus]),
       workdir: meson.current_source_dir())
endforeach
