headers = [
  'nl-wrapper.h',
  'ncsi.h',
  'ncsi_util.hpp'
]

sources = [
  'nl-wrapper.c',
  'ncsi_util.cpp'
]

libncsi_headers = ['.', '..']

libnl = dependency('libnl-3.0')

install_headers(
  headers,
  subdir: 'libncsi')

libncsi = library(
  'ncsi',
  sources,
  implicit_include_directories: false,
  include_directories: include_directories(libncsi_headers),
  dependencies: [libnl, dependency('phosphor-logging'), dependency('libnl-genl-3.0')],
  version: meson.project_version(),
  install: true)

libncsi = declare_dependency(
  include_directories: include_directories(libncsi_headers),
  link_with: libncsi)

import('pkgconfig').generate(
  name: 'libncsi',
  description: 'NCSI Netlink Protocol with C++ lib',
  version: meson.project_version(),
  libraries: libncsi)

