Installation
============
Install dependencies:

	$ sudo apt-get install libhiredis-dev pigpio

Enable pigpiod:

	$ sudo systemctl enable pigpiod.service

This starts pigpiod which gives all processes on localhost access to all GPIO
pins through a socket interface.

Compile:

	$ make

Install systemd service file:

	sudo cp telegraph-controller.service /etc/systemd/system
	sudo systemctl enable telegraph-controller.service

You might need to change the `ExecStart` and `User` properties in the
`.service` file to point to where the checkout lives.

License
=======
Copyright (C) 2014 by Matthew K. Roberts, KK5JY. All rights reserved.
Copyright (C) 2017 by Matthijs Kooijman

Licensed under GPL Version 3.0

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see: http://www.gnu.org/licenses/
