# Millennia-Laser-Control-App
Application for controlling and logging [Spectra-Physics Millennia Lasers](https://www.spectra-physics.com/f/millennia-ev-cw-dpss-laser).

![Laser image](https://raw.githubusercontent.com/alberti42/Millennia-Laser-Control-App/main/screenshots/Millennia_laser.png)

* It covers all functions provided by the app shipped originally with the laser.
* It provides new features. The most notable one is the possibility to log multiple parameters of the laser.
* It gives a feedback about the instantaneous RMS noise level of the laser (1s integration time).
* It circumvent the limitation of controlling a single laser when multiple lasers are connected.

# Compilation

The application has been tested and built using [MinGW-w64](https://www.mingw-w64.org/) compiler. If you are using [make](https://www.gnu.org/software/make/) provided by either [Cygwin](https://www.cygwin.com/) or [Msys2](https://www.msys2.org/) environment to process the `Makefile`, make sure that MinGW's g++ compiler can be found in Windows' path by editing the `$PATH` environment variable or specifying the exact path of MinGW's g++ compiler in the `Makefile`. The app has been tested under both Windows 7 and Windows 10.

# Automatic start after boot

You can start the application automatically at boot.



![Automatic start after boot](https://raw.githubusercontent.com/alberti42/Millennia-Laser-Control-App/main/screenshots/Screenshot_Millennia_laser_control_app.png)


![Automatic start after boot](https://raw.githubusercontent.com/alberti42/Millennia-Laser-Control-App/main/screenshots/Autostart_Millennia_laser_control_app.png)


# Credits

The application has been developed in C++ by [Andrea Alberti](http://quantum-technologies.iap.uni-bonn.de/alberti/).


