xsltproc_exe = find_program('xsltproc', required : get_option('man'))
want_man = (get_option('man').enabled() or get_option('man').auto()) and xsltproc_exe.found()
xsltproc_flags = [
        '--nonet',
        '--xinclude',
        '--stringparam', 'version', '@0@'.format(meson.project_version()),
        '--path',
        '@0@:@1@'.format(meson.current_build_dir(), meson.current_source_dir())]

custom_man_xsl = files('custom-man.xsl')

xslt_cmd = [xsltproc_exe, '-o', '@OUTPUT0@'] + xsltproc_flags

mandir1 = get_option('mandir') / 'man1'
mandir8 = get_option('mandir') / 'man8'

if xsltproc_exe.found()
custom_target('wall-bcst-send.1',
              input : 'wall-bcst-send.1.xml',
              output : 'wall-bcst-send.1',
              command : xslt_cmd + [custom_man_xsl, '@INPUT@'],
              install : want_man,
              install_dir : mandir1)
custom_target('wall-bcst-watcher.1',
              input : 'wall-bcst-watcher.1.xml',
              output : 'wall-bcst-watcher.1',
              command : xslt_cmd + [custom_man_xsl, '@INPUT@'],
              install : want_man,
              install_dir : mandir1)
custom_target('wall-bcst-watcher-gtk4.1',
              input : 'wall-bcst-watcher-gtk4.1.xml',
              output : 'wall-bcst-watcher-gtk4.1',
              command : xslt_cmd + [custom_man_xsl, '@INPUT@'],
              install : want_man,
              install_dir : mandir1)
custom_target('wall-bcst-gateway.1',
              input : 'wall-bcst-gateway.1.xml',
              output : 'wall-bcst-gateway.1',
              command : xslt_cmd + [custom_man_xsl, '@INPUT@'],
              install : want_man,
              install_dir : mandir1)
custom_target('wall-broadcaster.8',
              input : 'wall-broadcaster.8.xml',
              output : 'wall-broadcaster.8',
              command : xslt_cmd + [custom_man_xsl, '@INPUT@'],
              install : want_man,
              install_dir : mandir8)
endif
