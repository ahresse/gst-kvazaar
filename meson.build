project('gst-kvazaar', 'c',
  version : '0.1.0',
  meson_version : '>= 0.36.0',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

gst_version = '1.10.4'
version_arr = gst_version.split('.')
gst_version_major = version_arr[0].to_int()
gst_version_minor = version_arr[1].to_int()
gst_version_micro = version_arr[2].to_int()
if version_arr.length() == 4
  gst_version_nano = version_arr[3].to_int()
else
  gst_version_nano = 0
endif

glib_req = '>= 2.40.0'
gst_req = '>= @0@.@1@.0'.format(gst_version_major, gst_version_minor)

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')
host_system = host_machine.system()

cdata = configuration_data()

cdata.set_quoted('GST_API_VERSION', '1.0')
cdata.set_quoted('GST_PLUGIN_VERSION', meson.project_version())
cdata.set_quoted('GST_PLUGIN_LICENSE', 'LGPL')
cdata.set_quoted('GST_PLUGIN_ORIGIN', 'https://github.com/ahresse/gst-kvazaar')
cdata.set('HAVE_CPU_X86_64', host_machine.cpu() == 'amd64')
cdata.set('HAVE_CPU_ARM', host_machine.cpu() == 'arm')
cdata.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
cdata.set('SIZEOF_CHAR', cc.sizeof('char'))
cdata.set('SIZEOF_INT', cc.sizeof('int'))
cdata.set('SIZEOF_LONG', cc.sizeof('long'))
cdata.set('SIZEOF_SHORT', cc.sizeof('short'))
cdata.set('SIZEOF_VOIDP', cc.sizeof('void*'))
cdata.set_quoted('TARGET_CPU', 'target_cpu')
cdata.set_quoted('PACKAGE', 'none')

if get_option('with-crypto') == true
  cdata.set('HAS_CRYPTO', true)
endif

warning_flags = [
  '-Wmissing-declarations',
  '-Wredundant-decls',
  '-Wwrite-strings',
  '-Winit-self',
  '-Wmissing-include-dirs',
  '-Wno-multichar',
  '-Wvla',
  '-Wpointer-arith',
]

warning_c_flags = [
  '-Wmissing-prototypes',
  '-Wdeclaration-after-statement',
  '-Wold-style-definition',
  '-Waggregate-return',
]

foreach extra_arg : warning_flags
  add_project_arguments([extra_arg], language: 'c')
endforeach

foreach extra_arg : warning_c_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

# Mandatory GST deps
gst_dep = dependency('gstreamer-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_base_dep'])
#gstnet_dep = dependency('gstreamer-net-1.0', version : gst_req,
#  fallback : ['gstreamer', 'gst_net_dep'])
#gstcontroller_dep = dependency('gstreamer-controller-1.0', version : gst_req,
#  fallback : ['gstreamer', 'gst_controller_dep'])
#gstcheck_dep = dependency('gstreamer-check-1.0', version : gst_req,
#  required : get_option('tests'),
#  fallback : ['gstreamer', 'gst_check_dep'])
gstpbutils_dep = dependency('gstreamer-pbutils-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'pbutils_dep'])
#gstallocators_dep = dependency('gstreamer-allocators-1.0', version : gst_req,
#    fallback : ['gst-plugins-base', 'allocators_dep'])
#gstapp_dep = dependency('gstreamer-app-1.0', version : gst_req,
#    fallback : ['gst-plugins-base', 'app_dep'])
#gstaudio_dep = dependency('gstreamer-audio-1.0', version : gst_req,
#    fallback : ['gst-plugins-base', 'audio_dep'])
#gstfft_dep = dependency('gstreamer-fft-1.0', version : gst_req,
#    fallback : ['gst-plugins-base', 'fft_dep'])
#gstriff_dep = dependency('gstreamer-riff-1.0', version : gst_req,
#    fallback : ['gst-plugins-base', 'riff_dep'])
#gstrtp_dep = dependency('gstreamer-rtp-1.0', version : gst_req,
#    fallback : ['gst-plugins-base', 'rtp_dep'])
#gstrtsp_dep = dependency('gstreamer-rtsp-1.0', version : gst_req,
#    fallback : ['gst-plugins-base', 'rtsp_dep'])
#gstsdp_dep = dependency('gstreamer-sdp-1.0', version : gst_req,
#    fallback : ['gst-plugins-base', 'sdp_dep'])
#gsttag_dep = dependency('gstreamer-tag-1.0', version : gst_req,
#    fallback : ['gst-plugins-base', 'tag_dep'])
gstvideo_dep = dependency('gstreamer-video-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'video_dep'])

glib_deps = [dependency('glib-2.0', version : glib_req, fallback: ['glib', 'libglib_dep']),
             dependency('gobject-2.0', fallback: ['glib', 'libgobject_dep'])]
gio_dep = dependency('gio-2.0', fallback: ['glib', 'libgio_dep'])

gst_kvazaar_args = ['-DHAVE_CONFIG_H']
configinc = include_directories('.')

#pkgconfig = import('pkgconfig')
#plugins_pkgconfig_install_dir = join_paths(plugins_install_dir, 'pkgconfig')
#if get_option('default_library') == 'shared'
#  # If we don't build static plugins there is no need to generate pc files
#  plugins_pkgconfig_install_dir = disabler()
#endif

subdir('src')

configure_file(input : 'config.h.meson',
  output : 'config.h',
  configuration : cdata)
