gnome = import('gnome')

desktopdir = join_paths(datadir, 'applications')
icondir = join_paths(datadir, 'icons', 'hicolor', 'scalable', 'apps')
symbolicicondir = join_paths(datadir, 'icons', 'hicolor', 'symbolic', 'apps')
metainfodir = join_paths(datadir, 'metainfo')
man1dir = join_paths(mandir, 'man1')

conf = configuration_data()
conf.set('version', meson.project_version())
conf.set('version_date', version_date)

manpage = configure_file(
     input: files('piper.1.in'),
     output: 'piper.1',
     configuration: conf,
)

install_man(
     manpage,
     install_dir: man1dir,
)

about_dialog = configure_file(input: 'ui/AboutDialog.ui.in',
                              output: 'AboutDialog.ui',
                              configuration: conf)

install_data('org.freedesktop.GhostCAT.svg', install_dir: icondir)
install_data('org.freedesktop.GhostCAT-symbolic.svg', install_dir: symbolicicondir)

i18n.merge_file(input: 'org.freedesktop.GhostCAT.desktop.in',
                output: 'org.freedesktop.GhostCAT.desktop',
                type: 'desktop',
                po_dir: podir,
                install: true,
                install_dir: desktopdir)

appdata = configure_file(input: 'org.freedesktop.GhostCAT.appdata.xml.in.in',
                         output: 'org.freedesktop.GhostCAT.appdata.xml.in',
                         configuration: conf)

appdata = i18n.merge_file(input: appdata,
                          output: 'org.freedesktop.GhostCAT.appdata.xml',
                          type: 'xml',
                          po_dir: podir,
                          install: true,
                          install_dir: metainfodir)

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test(
    'validate appdata file',
    appstreamcli,
    args: ['validate', '--no-net', '--pedantic', appdata]
  )
endif

svg_mapping = files('svgs/svg-lookup.ini')

gresource = configure_file(input: 'piper.gresource.xml.in',
                           output: 'piper.gresource.xml',
                           command: ['generate-piper-gresource.xml.py',
                                     join_paths(meson.current_source_dir(), 'piper.gresource.xml.in'),
                                     join_paths(meson.current_build_dir(), 'piper.gresource.xml'),
                                     join_paths(meson.current_source_dir(), 'svgs')])

gnome.compile_resources('ghostcat', gresource,
                        source_dir: '.',
                        dependencies: [about_dialog],
                        gresource_bundle: true,
                        install: true,
                        install_dir: pkgdatadir)
