fw_update_test_src = declare_dependency(
          sources: [
            '../fw_inventory_manager.cpp',
            '../../common/utils.cpp',
            '../../pldmd/dbus_impl_requester.cpp',
            '../../pldmd/instance_id.cpp'])

tests = [
  'fw_inventory_manager_test',
]

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     link_args: dynamic_linker,
                     build_rpath: get_option('oe-sdk').enabled() ? rpath : '',
                     dependencies: [
                         fw_update_test_src,
                         gtest,
                         libpldm_dep,
                         phosphor_dbus_interfaces,
                         sdbusplus,
                         sdeventplus]),
       workdir: meson.current_source_dir())
endforeach
