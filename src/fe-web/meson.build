# fe-web module for irssi

fe_web_sources = [
  'fe-web.c',
  'fe-web-server.c',
  'fe-web-client.c',
  'fe-web-signals.c',
  'fe-web-utils.c',
  'fe-web-api.c',
]

# Build static library first (following irc module pattern)
libfe_web_a = static_library('fe_web',
  fe_web_sources,
  include_directories : rootinc,
  implicit_include_directories : false,
  dependencies : dep)

# Build shared module (following working module pattern)
shared_module('fe_web',
  name_suffix : module_suffix,
  install : true,
  install_dir : moduledir,
  link_whole : libfe_web_a)

# Install web static files
install_subdir('static',
  install_dir : get_option('datadir') / 'irssi' / 'web',
  strip_directory : true
)
