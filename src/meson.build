kvazaar_sources = [
	'gstkvazaarenc.c',
]

kvz_dep = dependency('kvazaar', version : '>=1.2.0', required : true)

if kvz_dep.found()
  gstkvazaarenc = library('gstkvazaarenc',
    kvazaar_sources,
    c_args : gst_kvazaar_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstvideo_dep, gstpbutils_dep, kvz_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
#  pkgconfig.generate(gstkvazaar, install_dir : plugin_pkgconfig_install_dir)
endif
