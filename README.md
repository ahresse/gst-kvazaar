Kvazaar encoder plugin
======================

What it is
----------

This is a video encoding element for GStreamer 1.0 based on Kvazaar, a LGPL HEVC
encoder.

Building with meson
-------------------

This installation guide has been tested under Debian 9.8.

1. Install dependencies:

- From the official repositories, meson and GStreamer:

 $ sudo apt install meson libgstreamer-plugins-base1.0-dev

- In order to install Kvazaar, follow the instructions on
https://github.com/ultravideo/kvazaar. (Make sure the resulting shared object
is under you LD_LIBRARY_PATH at runtime.)

2. To build gst-kvazaar plugin with meson, move to your local project directory and run
the following:

 $ meson build
 $ ninja -C build

Testing Pipelines
-----------------

 $ GST_PLUGIN_PATH=build/src gst-inspect-1.0 kvazaarenc
 $ GST_PLUGIN_PATH=build/src gst-launch-1.0 videotestsrc ! kvazaarenc ! avdec_h265 ! videoconvert ! fpsdisplaysink

Selective encryption features
-----------------------------

gst-kvazaar implements Kvazaar selective encryption features. Here is an example
pipeline of how to do it:

 $ GST_PLUGIN_PATH=build/src gst-launch-1.0 videotestsrc ! kvazaarenc crypto=on key=14,41,81,1,51,98,105,1,17,54,230,0,89,165,255,123 preset=ultrafast ! avdec_h265 ! autovideosink

The encrypted byte stream can be decrypted with OpenHEVC. An implementation of
the OpenHEVC decoder is available in gst-openhevc. Here is an example pipeline
showing how to use openhevcdec together with kvazaarenc:

 $ gst-launch-1.0 videotestsrc ! kvazaarenc crypto=on key=14,41,81,1,51,98,105,1,17,54,230,0,89,165,255,123 preset=ultrafast ! openhevcdec crypto=on key=14,41,81,1,51,98,105,1,17,54,230,0,89,165,255,123 ! xvimagesink

Note
----

Some part of the source code is in commentary because there are features still
not implemented.
