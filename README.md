## Introduction ##
I began C++ programming in 2001 as a sophomore majoring in electronic and information engineering.
In 2004 I took a graduate course in digital and signal processing, and began using C++ for industrial grade products.
After graduation in 2007, I became a professional software engineerer.
In the past 15 years, while keeped learning other languages such as Matlab and Python, C++ is what I earned my living on.
I have been coding in quite a few domain, but mostly in signal/image/audio/video processing, image/video codec, media demuxer/muxer, media playing/transcoding, multimedia streaming and 3D rendering.

I want to share what I have done, hoping others will find them useful.
Of course I can't just paste what I have coded before, because the company/institute own the copyright, not me, and I have lost most of the codes written many years ago.
What I plan to do here is to redesign and reimplement the most valuable things(for me and hopefully for you) which I have done in the past 15 years.
With morden C++ and more skilled design/coding technique, I think I can do much things more elegantly.

What I'm planning to do now(2016-10-1) is:
* an utility library (memory management, string processing, xml/json parsing, thread-safe queue, etc.)
* a signal and image processing library (image resizing, image enhancement, image segmentation, etc.)
* a video codec library (H.264, HEVC, MPEG-2, DNxHD/DNxHR, AVS+, etc.)
* a media demuxer/muxer library (MP4, MKV, WebM, TS, MXF, etc.)
* a media playing/transcoding engine
* a media player/transcoder
* a network library (socket wrapper, http, websocket, media streaming, etc.)

Well, it really is a wishlist.
I can only do this project in part-time, so the progress may be(and should be) slow.
I don't know when I can finish it, I won't promise anything.

If you find any bug, please contact me at <illigle@163.com>

## Compiler requirment ##
You need a C++ compiler supporting most of C++14 features, some common candidates are:
* Visual Studio 2015 or above
* GCC 5.0 or above
* Clang 3.5 or above

## Other tools ##
* CMake 3.5 or above

## Supported operating systems ##
* Windows 7 or above
* Linux with kernel 3.2 or above

> I want to support MacOS, but I don't have a Mac computer, if you find something broken on Mac, please send me a pull request.

## Supported architecture ##
The siganl/image processing library and video/audio codec will only support x86/x64 architecture, because I will use a lot of SSE/AVX intrinsics and assembly codes.

## License ##
This project is licensed under the [Mozilla Public License Version 2.0](http://mozilla.org/MPL/2.0).