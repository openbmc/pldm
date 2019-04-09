gtest = dependency('gtest', main: true, required: false)
gmock = dependency('gmock', required: false)
pldm_includes = include_directories('../')
tests = [
 'libpldm_base_test.cpp',
 'libpldmresponder_base_test.cpp'
 ]
foreach t : tests
 test(t,executable(t.underscorify(), t,
      include_directories : pldm_includes,
      link_with: [libpldm_lib,libpldmresponder_lib],
      dependencies: [gtest,gmock]))
endforeach

