
libpldm_sources += files(
  'base.c',
  'bios.c',
  'platform.c',
  'bios_table.c',
  'firmware_update.c',
  'fru.c',
  'pdr.c',
  'utils.c'
  )

if get_option('oem-ibm').enabled()
   subdir('oem/ibm')
endif

if get_option('requester-api').enabled()
   subdir('requester')
endif


#libpldm_dep = declare_dependency(
#  include_directories: include_directories(libpldm_headers),
#  link_with: libpldm)

#import('pkgconfig').generate(
#  name: 'libpldm',
#  description: 'PLDM protocol encode/decode C lib',
#  version: meson.project_version(),
#  libraries: libpldm)


