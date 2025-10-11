# this file is part of irssi

shared_module('fe_web',
  files(
    'fe-web.c',
    'fe-web-client.c',
    'fe-web-server.c',
    'fe-web-signals.c',
    'fe-web-utils.c',
  )
  + [ irssi_version_h ],
  include_directories : rootinc,
  implicit_include_directories : false,
  name_suffix : module_suffix,
  install : true,
  install_dir : moduledir,
  dependencies : dep,
)

# noinst_headers = files(
#   'fe-web.h',
#   'module.h',
# )
