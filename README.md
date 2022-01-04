# Millennia-Laser-Control-App
Application for controlling and logging [Spectra-Physics Millennia Lasers](https://www.spectra-physics.com/f/millennia-ev-cw-dpss-laser).

![Laser image](https://raw.githubusercontent.com/alberti42/Millennia-Laser-Control-App/main/screenshots/Millennia_laser.png)

* It covers all functions provided by the app shipped originally with the laser.
* It provides new features. The most notable one is the possibility to log multiple parameters of the laser.
* It gives a feedback about the instantaneous RMS noise level of the laser (5s integration time).
* It allows controlling multiple lasers when these are connected to the same computer via USB.

# Instructions

The screenshot below shows two windows next to each other, controlling two different Millennia lasers. The GUI is mostly self-explanatory. However, a few points must be noted:

* The power and other important parameters of the lasers are queried every 100 ms, and shown instantaneously to the user through the GUI.
* The values of the power recorded in the past 5 seconds are used to compute the RMS noise, normalized to the average intensity. This value is the relative intensity noise (RIN) of the laser. Thus, the app gives an estimate of the RIN measured in the low-frequency bandwidth corresponding to the interval [0.2-10] Hz.
* The laser itself has a readout noise at the level of about 0.02%. This can be clearly see when the laser is switched off and yet the recorded power fluctuates.
* The logging period determines how often the recorded information are stored on the log file. The user can choose the path of the log file from the field *Log path*. The RMS power is averaged over all values computed during the past logging period in order to produce a more reliable estimate of the laser RIN.
* The *Command line* field allows the user to give commands to the laser or to query its status. Here are few examples: the command `P:25` sets the laser power to 25W. The command `?FH` shows the history of errors thrown by the laser in the past. For the complete list of commands, refer to the user manual of the laser.
* If the laser throws a new error while the *Millennia Control App* is logging, the error will be recorded and displayed with a red text appearing on the bottom of the window. The error will also be stored on the log file so to record the timestamp.
* Errors while operating the app are signaled by a message box. E.g., if the user attempts to connect to a laser having a wrong serial number, an error message is displayed.

![Automatic start after boot](https://raw.githubusercontent.com/alberti42/Millennia-Laser-Control-App/main/screenshots/Screenshot_Millennia_laser_control_app.png)


# App launch and automatic start

It may be convenient to create a launcher link to run the app as shown in the screenshot:

![Automatic start after boot](https://raw.githubusercontent.com/alberti42/Millennia-Laser-Control-App/main/screenshots/Autostart_Millennia_laser_control_app.png)


In the *Target* field, the user can specify few options:

* The option `-config "the path to the config file"` sets the *config* file used by the app. Otherwise, the app opens the file `config.txt` from the specified work directory.
* The option `-start` instructs the app to start immediately logging once opened.

# *Config* file

When the settings are changed, the new settings are automatically stored on the *config* file `config.txt` in the work directory or in the config file specified by the `-config` option. The *config* file can also be edited with a text editor. Here below is an example of *config* file:

```
logFilePath = C:\Users\DQSIM\Documents\Millennia Software\Log files\log_2675.txt
logPeriod = 30
comPort = COM6
laserName = Lattice laser
serialNumber = 2675
```

# *Log* file

Here below is an example of log file.

```
# Timestamp		Power	RMS	Avg.RMS	T-dio.	T.cry.	Dio.c.	Dio.h.	Head-h.	Las.-h.	Error
2019-12-11 12:54:41	10.05	0.0457	0.0737	23.20	21.63	7.484	64.7	64.7	1346.7	000
2019-12-11 12:55:11	10.06	0.0482	0.0581	23.21	21.60	7.481	64.7	64.7	1346.7	000
2019-12-11 12:55:41	10.06	0.0456	0.0515	23.21	21.63	7.482	64.7	64.7	1346.7	000
2019-12-11 12:56:11	10.04	0.0619	0.0644	23.21	21.65	7.481	64.7	64.7	1346.7	000
2019-12-11 12:56:41	10.05	0.0750	0.0716	23.21	21.63	7.479	64.7	64.7	1346.7	000
2019-12-11 12:57:11	10.05	0.0599	0.0663	23.21	21.63	7.481	64.7	64.7	1346.7	000
2019-12-11 12:57:41	10.06	0.0887	0.0683	23.21	21.63	7.479	64.7	64.7	1346.7	000
```

The header in the *log* file is printed every time the logging operation is resumed.

# Compilation

The application has been tested and built using [MinGW-w64](https://www.mingw-w64.org/) compiler. If you are using [make](https://www.gnu.org/software/make/) provided by either [Cygwin](https://www.cygwin.com/) or [Msys2](https://www.msys2.org/) environment to process the *Makefile*, make sure that MinGW's g++ compiler can be found in Windows' path by editing the `$PATH` environment variable or specifying the exact path of MinGW's g++ compiler in the *Makefile*. The app has been tested under both Windows 7 and Windows 10.

# Debug

For debug purpose, use the command

```
DEBUG=1 make
```

to build the app. The app thus compiled prints status messages to the terminal.


# Credits

The application has been developed in C++ by [Andrea Alberti](http://quantum-technologies.iap.uni-bonn.de/alberti/).


