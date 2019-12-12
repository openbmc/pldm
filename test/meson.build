if get_option('oe-sdk').enabled()
  # Setup OE SYSROOT
  OECORE_TARGET_SYSROOT = run_command('sh', '-c', 'echo $OECORE_TARGET_SYSROOT').stdout().strip()
  if OECORE_TARGET_SYSROOT == ''
      error('Unable to get $OECORE_TARGET_SYSROOT, check your environment.')
  endif
  message('OE_SYSROOT: ' + OECORE_TARGET_SYSROOT)
  rpath = ':'.join([OECORE_TARGET_SYSROOT + '/lib', OECORE_TARGET_SYSROOT + '/usr/lib'])
  ld_so = run_command('sh', '-c', 'find ' + OECORE_TARGET_SYSROOT + '/lib/ld-*.so | sort -r -n | head -n1').stdout().strip()
  dynamic_linker = ['-Wl,-dynamic-linker,' + ld_so]
else
  dynamic_linker = []
endif

gtest = dependency('gtest', main: true, disabler: true, required: true)
gmock = dependency('gmock', disabler: true, required: true)
pldmd = declare_dependency(sources: '../instance_id.cpp')

tests = [
  'libpldm_base_test',
  'libpldm_platform_test',
  'libpldmresponder_base_test',
  'libpldm_bios_test',
  'libpldm_bios_table_test',
  'libpldmresponder_bios_test',
  'libpldmresponder_pdr_state_effecter_test',
  'libpldmresponder_bios_table_test',
  'libpldmresponder_platform_test',
  'libpldm_fru_test',
  'libpldm_utils_test',
  'pldmd_instanceid_test',
  'pldmd_registration_test',
]

if get_option('oem-ibm').enabled()
  tests += [
    '../oem/ibm/test/libpldm_fileio_test',
    '../oem/ibm/test/libpldmresponder_fileio_test'
  ]
endif

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     link_args: dynamic_linker,
                     build_rpath: get_option('oe-sdk').enabled() ? rpath : '',
                     dependencies: [
                         libpldm,
                         libpldmresponder,
                         gtest,
                         gmock,
                         pldmd,
                         dependency('phosphor-dbus-interfaces'),
                         dependency('sdbusplus')]),
       workdir: meson.current_source_dir())
endforeach
