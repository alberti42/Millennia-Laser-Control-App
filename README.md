# Millennia-Laser-Control-App
Application for controlling and logging [Spectra-Physics Millennia Lasers](https://www.spectra-physics.com/f/millennia-ev-cw-dpss-laser).

![Laser image](https://raw.githubusercontent.com/alberti42/Millennia-Laser-Control-App/main/screenshots/Millennia_laser.png)

* It covers all functions provided by the app shipped originally with the laser.
* It provides new features. The most notable one is the possibility to log multiple parameters of the laser.
* It gives a feedback about the instantaneous RMS noise level of the laser (1s integration time).
* It circumvent the limitation of controlling a single laser when multiple lasers are connected.

# Compilation

The application has been tested and compiled with both Cygwin and Msys2 environment. The recommended compiler is [MinGW-w64](https://www.mingw-w64.org/) with standalone installation. Make sure that MinGW's g++ compiler can be found in Windows' path by editing the `$PATH` environment variable or edit the `Makefile` to specify the exact path of MinGW's g++ compiler.

# Automatic start after boot

You can start the application automatically at boot.


# Credits

The application has been written by [Andrea Alberti](http://quantum-technologies.iap.uni-bonn.de/alberti/).


