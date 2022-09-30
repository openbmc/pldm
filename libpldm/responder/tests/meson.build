tests = [
  'pldm_base_responder_test',
]

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     dependencies: [
                         libpldm_dep,
                         gtest,
                         gmock]),
       workdir: meson.current_source_dir())
endforeach

