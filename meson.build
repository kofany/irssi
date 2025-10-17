project('irssi-fe-web', 'c',
  version : '1.0.0',
  license : 'GPL-2.0',
  meson_version : '>=0.53'
)

# Dependencies
irssi_dep = dependency('irssi-1', required : true)
glib_dep = dependency('glib-2.0', version : '>=2.32', required : true)
openssl_dep = dependency('openssl', version : '>=3.0', required : true)

# Include directories
inc = include_directories('.')

# Source files
fe_web_sources = files(
  'src/fe-web.c',
  'src/fe-web-server.c',
  'src/fe-web-ssl.c',
  'src/fe-web-crypto.c',
  'src/fe-web-client.c',
  'src/fe-web-signals.c',
  'src/fe-web-netserver.c',
  'src/fe-web-utils.c',
  'src/fe-web-json.c',
  'src/fe-web-websocket.c',
)

# Build shared module
shared_module('fe_web',
  sources : fe_web_sources,
  dependencies : [irssi_dep, glib_dep, openssl_dep],
  name_prefix : 'lib',
  name_suffix : 'so',
  install : true,
  install_dir : get_option('prefix') / get_option('libdir') / 'irssi' / 'modules'
)

# Install documentation
install_data(
  'docs/CLIENT-SPEC.md',
  'docs/PROTOCOL.md',
  'docs/ARCHITECTURE.md',
  install_dir : get_option('prefix') / get_option('datadir') / 'doc' / 'irssi-fe-web'
)

message('')
message('*** Irssi FE-Web Module v' + meson.project_version() + ' configured ***')
message('')
message('Install prefix ................... : ' + get_option('prefix'))
message('Module install directory ......... : ' + get_option('prefix') / get_option('libdir') / 'irssi' / 'modules')
message('')
message('Now run: ninja -C ' + meson.project_build_root() + ' to build')
message('Then run: ninja -C ' + meson.project_build_root() + ' install to install')
message('')
