project(
  'wall-broadcaster',
  'c',
  meson_version : '>= 0.61.0',
  default_options : [
                  'prefix=/usr',
                  'sysconfdir=/etc',
                  'localstatedir=/var',
                  'buildtype=debugoptimized',
  		  'default_library=shared',
		  'b_pie=true',
                  'b_lto=true',
		  'warning_level=2'],
  license : ['GPL-2.0-or-later'],
  version : '0.1.0',
)

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())

cc = meson.get_compiler('c')
pkg = import('pkgconfig')
inc = include_directories(['include'])

add_project_arguments(['-D_GNU_SOURCE=1',
		       '-DXTSTRINGDEFINES',
		       '-D_FORTIFY_SOURCE=2',
                       '-D_FILE_OFFSET_BITS=64',
                       '-D_TIME_BITS=64'], language : 'c')

possible_cc_flags = [
		  '-fstack-protector-strong',
		  '-funwind-tables',
		  '-fasynchronous-unwind-tables',
		  '-fstack-clash-protection',
		  '-Werror=return-type',
		  '-Wbad-function-cast',
		  '-Wcast-align',
                  '-Werror',
                  '-Wformat-security',
		  '-Wmissing-declarations',
		  '-Wmissing-prototypes',
		  '-Wnested-externs',
		  '-Wshadow',
		  '-Wstrict-prototypes',
		  '-Wundef',
		  ]
add_project_arguments(cc.get_supported_arguments(possible_cc_flags), language : 'c')

# installation directories
prefixdir = get_option('prefix')
if not prefixdir.startswith('/')
        error('Prefix is not absolute: "@0@"'.format(prefixdir))
endif
libexecdir = get_option('libexecdir')
systemunitdir = prefixdir / 'lib/systemd/system'
dbuspolicydir = prefixdir / 'share/dbus-1/system.d'

libsystemd = dependency('libsystemd', required : true)

wall_broadcaster_c = ['src/wall-broadcaster.c']
executable('wall-broadcaster',
           wall_broadcaster_c,
           include_directories : inc,
	   dependencies : [libsystemd],
	   install_dir : libexecdir,
           install : true)

wall_bcst_watcher_c = ['src/wall-bcst-watcher.c']
executable('wall-bcast-watcher',
           wall_bcst_watcher_c,
           include_directories : inc,
           dependencies : [libsystemd],
           install : true)

configure_file(output: 'config.h', configuration: conf)

#subdir('man')
subdir('units')
subdir('dbus')
#subdir('tests')
