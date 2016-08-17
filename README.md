icetop
======

This is a simple, console-only monitor for
[Icecream](https://github.com/icecc/icecream). Also, I am using it as a
playground for [libdill](http://libdill.org), testing C++14 features
(and C++1z, when compilers implementing it), and experimenting with text
console output.


Screenshot
----------

![Screenshot](https://github.com/aperezdc/icetop/raw/master/icetop.gif)


Building
--------

You will need the development headers for Icecream and
[libdill](http://libdill.org) installed:

```sh
./configure && make
```

Or, if you are using the sources from Git:

```sh
autoreconf && ./configure && make
```

If your system does not have pacakages for `libdill`, you can build it
yourself and link it statically in `icetop`:

```sh
# Build libdill locally
wget -O - http://libdill.org/libdill-0.5-beta.tar.gz | tar -xzf -
pushd libdill-0.5-beta
./configure --prefix=$(pwd)/../libdill --enable-static --disable-shared
make install
popd
# Build icetop using the local libdill build
./configure PKG_CONFIG_PATH=$(pwd)/libdill/lib/pkgconfig
make
```

License
-------

`icetop` is distributed under the terms of the GPLv2 license. See
[COPYING](COPYING) for the complete text of the license.
