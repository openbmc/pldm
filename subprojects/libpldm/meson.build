project('libpldm', ['c','cpp'],
    default_options: [
      'buildtype=debugoptimized',
      'warning_level=3',
      'werror=true',
      'cpp_std=c++20',
      'tests=' + (meson.is_subproject() ? 'disabled' : 'enabled'),
    ],
    version: '1.0.0',
    meson_version: '>=0.63.0',
)

libpldm_sources = files()
subdir('src')

libpldm_include_dir = ['include']
libpldm_headers = files()

subdir('include/libpldm')

libpldm = library(
  'pldm',
   libpldm_sources,
   implicit_include_directories: false,
   include_directories: libpldm_include_dir,
   version: meson.project_version(),
   install: true
   )

install_headers(
  libpldm_headers,
  preserve_path: true,
  subdir:'libpldm'
  )

libpldm_dep = declare_dependency(
  include_directories: libpldm_include_dir,
  link_with: libpldm)

import('pkgconfig').generate(
  name: 'libpldm',
  description: 'PLDM protocol encode/decode C lib',
  version: meson.project_version(),
  libraries: libpldm)

if get_option('tests').enabled()
  subdir('tests')
endif
