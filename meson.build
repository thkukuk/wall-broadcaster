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
                  'werror=true',
		  'warning_level=2'],
  license : ['GPL-2.0-or-later'],
  version : '0.3.1',
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
		  '-Wcast-align',
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
libgtk4 = dependency('gtk4', required: get_option('gtk4'))

wall_broadcaster_c = ['src/wall-broadcaster.c', 
                      'src/wall-broadcaster-varlink.c',
                      'src/varlink-org.openSUSE.wall-broadcaster.c']
executable('wall-broadcaster',
           wall_broadcaster_c,
           include_directories : inc,
	   dependencies : [libsystemd],
	   install_dir : libexecdir,
           install : true)

wall_bcst_watcher_c = ['src/wall-bcst-watcher.c']
executable('wall-bcst-watcher',
           wall_bcst_watcher_c,
           include_directories : inc,
           dependencies : [libsystemd],
           install : true)

wall_bcst_gateway_c = ['src/wall-bcst-gateway.c']
executable('wall-bcst-gateway',
           wall_bcst_gateway_c,
           include_directories : inc,
           dependencies : [libsystemd],
           install : true)

wall_bcst_send_c = ['src/wall-bcst-send.c']
executable('wall-bcst-send',
           wall_bcst_send_c,
           include_directories : inc,
           dependencies : [libsystemd],
           install : true)

if libgtk4.found()
wall_bcst_watcher_gtk4_c = ['src/wall-bcst-watcher-gtk4.c']
executable('wall-bcst-watcher-gtk4',
           wall_bcst_watcher_gtk4_c,
           include_directories : inc,
           dependencies : [libgtk4],
           install : true)
endif

configure_file(output: 'config.h', configuration: conf)

subdir('man')
subdir('units')
subdir('dbus')
subdir('desktop')
#subdir('tests')
