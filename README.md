This is a port of Decawave's RTLS PC application for the Linux platform. You need the following extra packages on Debian to compile:

* qt5-qmake
* qtbase5-dev
* libqt5serialport5-dev
* libblas-dev
* liblapack-dev

To compile, use `qmake` (or `qmake -qt=qt5` if you have multiple versions of QT on your system), then `make`.
The resulting executable would be `DecaRangeRTLS`.

Based on [TREK1000 source code](https://www.decawave.com/software/), DecaRTLS 3.8.
Changes to the original code:

* Fixed USB device name string
* Changed QT project and removed Windows Armadillo libraries
