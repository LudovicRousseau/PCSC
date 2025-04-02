# for ninja:
# meson setup builddir
# cd builddir ; meson compile

# fox Xcode:
# meson setup --backend=xcode xcode
# cd xcode ; meson compile

# regenerate configuration (run from builddir/)
# meson setup --reconfigure --wipe

project('pcsc-lite', 'c',
  meson_version : '>=0.58.0',
  version : '2.3.3')

# for config.h
conf_data = configuration_data({
  'VERSION' : '"' + meson.project_version() + '"',
  'PCSCLITE_HP_DROPDIR' : '"' + get_option('usbdropdir') + '"',
  'HAVE_NANOSLEEP' : true,
  'NO_LOG' : get_option('embedded'),
  })
# for generated files from .in templates
sbindir = join_paths(get_option('prefix') , get_option('sbindir'))
confgen_data = configuration_data({
  'VERSION' : meson.project_version(),
  'ipcdir' : get_option('ipcdir'),
  'sysconfdir' : get_option('sysconfdir'),
  'sbindir_exp' : sbindir,
  'PCSCLITE_CONFIG_DIR' : get_option('serialconfdir'),
  'usbdropdir' : get_option('usbdropdir'),
  })

# tests for functions
compiler = meson.get_compiler('c')
if compiler.has_function('getrandom', prefix : '#include <sys/random.h>')
  conf_data.set('HAVE_GETRANDOM', true)
endif
if compiler.has_function('secure_getenv')
  conf_data.set('HAVE_SECURE_GETENV', true)
endif
if compiler.has_function('getopt_long')
  conf_data.set('HAVE_GETOPT_LONG', true)
endif

# tests for options
features = []
if get_option('usb')
  conf_data.set('USE_USB', true)
  features += 'USB'
endif

if get_option('serial')
  conf_data.set('USE_SERIAL', true)
  features += 'serial'
endif

if get_option('filter_names')
  conf_data.set('FILTER_NAMES', true)
  features += 'filter_names'
endif

# flex generator
gen_flex = generator(find_program('flex'),
  output : '@BASENAME@.c',
  arguments : ['-o', '@OUTPUT@', '--prefix=@BASENAME@', '@INPUT@'])

# global arguments
add_global_arguments('-fvisibility=hidden', language : 'c')

# pcscd daemon
pcscd_src = [
  'src/atrhandler.c',
  'src/auth.c',
  'src/debuglog.c',
  'src/dyn_unix.c',
  'src/eventhandler.c',
  'src/hotplug_generic.c',
  'src/hotplug_libudev.c',
  'src/hotplug_libusb.c',
  'src/ifdwrapper.c',
  'src/pcscdaemon.c',
  'src/prothandler.c',
  'src/readerfactory.c',
  'src/simclist.c',
  'src/sys_unix.c',
  'src/utils.c',
  'src/winscard.c',
  'src/winscard_msg.c',
  'src/winscard_msg_srv.c',
  'src/winscard_svc.c'
  ]
gen_src = gen_flex.process('src/configfile.l', 'src/tokenparser.l')
pcscd_src += gen_src
incdir = include_directories(['src', 'src/PCSC'])

# dependencies
threads_dep = dependency('threads')
pcscd_dep = [threads_dep]
if get_option('libudev')
  udev_dep = dependency('libudev')
  pcscd_dep += udev_dep
  conf_data.set('HAVE_LIBUDEV', true)
  features += 'libudev'
endif

if get_option('libusb')
  if get_option('libudev')
    error('You can\'t use both libudev and libusb')
  endif
  libusb_dep = dependency('libusb-1.0')
  pcscd_dep += libusb_dep
  conf_data.set('HAVE_LIBUSB', true)
  features += 'libusb'
endif

cc = meson.get_compiler('c')
dl_deps = cc.find_library('dl', required: false)

pcscd_dep += dl_deps

if get_option('polkit')
  polkit_dep = dependency('polkit-gobject-1')
  pcscd_dep += polkit_dep
  conf_data.set('HAVE_POLKIT', true)
  confgen_data.set('requires_polkit', 'Requires=polkit.service')
  features += 'polkit'
endif

if get_option('libsystemd')
  systemd_dep = dependency('libsystemd')
  pcscd_dep += systemd_dep
  conf_data.set('USE_LIBSYSTEMD', true)
  features += 'systemd'
endif

# architecture
r = run_command('uname', check: true)
pcsc_arch = r.stdout().strip()
if pcsc_arch == 'Darwin'
  pcsc_arch = 'MacOS'
  pcscd_dep += dependency('appleframeworks', modules : ['foundation', 'IOKit'])
  pcscd_src += files(['src/hotplug_macosx.c', 'src/dyn_macosx.c'])
endif
conf_data.set_quoted('PCSC_ARCH', pcsc_arch)

features +=  pcsc_arch
features += target_machine.cpu()
features += 'ipcdir=' + get_option('ipcdir')
features += 'usbdropdir=' + get_option('usbdropdir')
features += 'serialconfdir=' + get_option('serialconfdir')

# generate PCSCLITE_FEATURES
conf_data.set('PCSCLITE_FEATURES', '"' + ' '.join(features) + '"')

executable('pcscd',
  sources : pcscd_src,
  include_directories : incdir,
  dependencies : pcscd_dep,
  c_args: '-DPCSCD',
  export_dynamic : true,
  install_dir : sbindir,
  install : true)

# libpcsclite_real library
libpcsclite_real_src = [
  'src/debug.c',
  'src/winscard_clnt.c',
  'src/simclist.c',
  'src/sys_unix.c',
  'src/utils.c',
  'src/winscard_msg.c'
  ]
shared_library('pcsclite_real',
  libpcsclite_real_src,
  include_directories : incdir,
  dependencies : threads_dep,
  c_args: '-DLIBPCSCLITE -DSIMCLIST_NO_DUMPRESTORE',
  soversion : 1,
  install : true)

# libpcsclite library
libpcsclite_src = [
  'src/error.c',
  'src/g_defines.c',
  'src/libredirect.c',
  'src/sys_unix.c'
  ]
libpcsclite = shared_library('pcsclite',
  libpcsclite_src,
  dependencies : dl_deps,
  include_directories : incdir,
  soversion : 1,
  install : true)

# static library
libpcsclite_static_src = libpcsclite_real_src + [
  'src/error.c',
  'src/g_defines.c',
  ]
libpcsclite_static = static_library('pcsclite',
  libpcsclite_static_src,
  include_directories : incdir,
  dependencies : threads_dep,
  c_args: '-DLIBPCSCLITE -DSIMCLIST_NO_DUMPRESTORE',
  install : true)

# libpcsclite_fake library
library('pcsclite_fake',
  sources : 'src/libfake.c',
  include_directories : incdir)

# libpcscspy library
library('pcscspy',
  sources : ['src/spy/libpcscspy.c', 'src/sys_unix.c'],
  dependencies : dl_deps,
  include_directories : incdir,
  soversion : 0,
  install : true)

install_data('src/spy/pcsc-spy',
  install_dir : get_option('bindir'))

install_data('src/spy/setup_spy.sh',
  install_dir : 'share/doc/pcsc-lite')

run_command('pod2man',
  ['--date=2024-01-01', 'src/spy/pcsc-spy.pod', 'pcsc-spy.1'],
  check : true)
install_data('pcsc-spy.1',
  install_dir : join_paths(get_option('mandir'), 'man1'))

# testpcsc program
executable('testpcsc',
  sources : 'src/testpcsc.c',
  include_directories : incdir,
  link_with : libpcsclite)

executable('pcsc_demo',
  sources : 'doc/example/pcsc_demo.c',
  include_directories : incdir,
  link_with : libpcsclite)

executable('pcsc_demo_static',
  sources : 'doc/example/pcsc_demo.c',
  include_directories : incdir,
  link_with : libpcsclite_static)

# PC/SC headers
install_headers(
  ['src/PCSC/debuglog.h',
   'src/PCSC/ifdhandler.h',
   'src/PCSC/reader.h',
   'src/PCSC/winscard.h',
   'src/PCSC/wintypes.h'],
  install_dir : get_option('includedir') / 'PCSC')

# data
if get_option('polkit')
install_data('doc/org.debian.pcsc-lite.policy',
  install_dir : polkit_dep.get_variable('policydir'))
endif

install_data('etc/fr.apdu.pcsclite.metainfo.xml',
  install_dir : 'share/metainfo'
  )

install_data('etc/pcscd',
  install_dir : get_option('sysconfdir') / 'default'
  )

# generate config.h
configure_file(output : 'config.h',
  configuration : conf_data)

# generate from .in files
configure_file(output : 'pcsclite.h',
  input : 'src/PCSC/pcsclite.h.in',
  install_dir : get_option('prefix') / get_option('includedir') / 'PCSC',
  configuration : confgen_data)
configure_file(output : 'pcscd.h',
  input : 'src/pcscd.h.in',
  configuration : confgen_data)
if get_option('libsystemd')
  systemd = dependency('systemd')
  unit = get_option('systemdunit')
  systemdsystemunitdir = systemd.get_variable(pkgconfig : 'systemd' + unit + 'unitdir')
configure_file(output : 'pcscd.socket',
  input : 'etc/pcscd.socket.in',
  install_dir : systemdsystemunitdir,
  configuration : confgen_data)
configure_file(output : 'pcscd.service',
  input : 'etc/pcscd.service.in',
  install_dir : systemdsystemunitdir,
  configuration : confgen_data)
endif
configure_file(output : 'pcscd.8',
  input : 'doc/pcscd.8.in',
  install_dir : join_paths(get_option('mandir'), 'man8'),
  configuration : confgen_data)
if get_option('serial')
configure_file(output : 'reader.conf.5',
  input : 'doc/reader.conf.5.in',
  install_dir : join_paths(get_option('mandir'), 'man5'),
  configuration : confgen_data)
endif

# pkg-config libpcsclite.pc
pkg = import('pkgconfig')
pkg.generate(
  libraries : '-L${libdir} -lpcsclite',
  libraries_private : '-pthread',
  subdirs : 'PCSC',
  version : meson.project_version(),
  name : 'PCSC Lite',
  filebase : 'libpcsclite',
  variables : [
    'usbdropdir=' + conf_data.get_unquoted('PCSCLITE_HP_DROPDIR'),
    'serialconfdir=' + get_option('serialconfdir')],
  description : 'PC/SC smart card interface')

# generate documentation
configure_file(output : 'doxygen.conf',
  input : 'doc/doxygen.conf.in',
  configuration : confgen_data)
doxygen = find_program('doxygen', required : false)
if doxygen.found()
  message('Doxygen found')
  run_target('doc', command : [doxygen, 'doxygen.conf'])
else
  warning('Documentation disabled without doxygen')
endif
