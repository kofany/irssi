project('irssi-awl', 'c')

irssi_src = get_option('irssi_src', fallback: '')
if irssi_src == ''
  irssi_src = '/workspace/ext/irssi'
endif

cc = meson.get_compiler('c')
glib_dep = dependency('glib-2.0')
gmodule_dep = dependency('gmodule-2.0')

inc = include_directories(irssi_src, irssi_src + '/src')

srcs = files('src/awl_core.c')

shared_module('awl_core',
  srcs,
  include_directories: inc,
  dependencies: [glib_dep, gmodule_dep],
  name_prefix: '',
  install: false
)