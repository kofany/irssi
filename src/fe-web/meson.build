# fe-web module for irssi

fe_web_sources = [
  'fe-web.c',
  'fe-web-server.c',
  'fe-web-client.c',
  'fe-web-signals.c',
  'fe-web-utils.c',
  'fe-web-api.c',
]

# Check for OpenSSL (required for WebSocket handshake)
openssl_dep = dependency('openssl', required: true)

# Build fe-web as a shared module
shared_module('fe_web',
  fe_web_sources,
  name_suffix : module_suffix,
  install : true,
  install_dir : moduledir,
  include_directories : rootinc,
  implicit_include_directories : false,
  dependencies : dep + [openssl_dep],
  link_with : [
    libcore_a,
    libfe_common_core_a,
  ],
  c_args : [
    def_helpdir,
    def_themesdir,
  ]
)

# Install web static files
install_subdir('static',
  install_dir : get_option('datadir') / 'irssip' / 'web',
  strip_directory : true
)
