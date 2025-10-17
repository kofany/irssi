project('irssi-fe-web', 'c',
  version : '1.0.0',
  license : 'GPL-2.0',
  meson_version : '>=0.53'
)

# Dependencies
dep = dependency('irssi-1', required : true)
glib_dep = dependency('glib-2.0', version : '>=2.32', required : true)

# Include directories
rootinc = include_directories('.')

# Module suffix
module_suffix = 'so'

# Install directory
moduledir = get_option('prefix') / get_option('libdir') / 'irssi' / 'modules'

# Version header (dummy for standalone module)
irssi_version_h = []

# Build module from src/
subdir('src')

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
