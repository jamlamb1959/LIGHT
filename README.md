# LIGHT

This is a simple ESP32 program based on using platformio to build.

# BUILD

So there is a make file with several labels.

- make alone should cause the program to rebuild, copy it to a public repository and issue a MQTT event that
will cause any instances that are running to reload.

- make push - does the same.
- make upload - will force an upload ot the compiled program to a connected device.
- make mu - will make, upload and then monitor the local device.
- make monitor - this will attach to the serial connection to the local device and monitor the output.


You need to get your platformio environment setup correctly. If you don't know what that means this might
not be a good project for you.

# Overview

I'm trying to understand how MQTT can be used by the JMRI program to control a model train set.
To this end the esp32 will boot and then scan the WiFi network.  this will return a list of SSID's that are
seen by the device.  the list is sorted from strongest to weekest signal.  the program will have a list of SSID and passwords that it is allowed to try.  The program will attempt to connect to the WiFi networks in order from strongest to weekest.  for the SSID in the scan entry it will attempt to find the PASSWORD in the password table and then attempt to connect.  Once it succeeds it is done.  if I can't find a usable connection it will delay 1 minute and they start over.

Once connected the next procedure is to make sure that the program is running the latest software.  To accomplish this check the program will attempt to pull the latest code from the web repository.  A PHP script is executed on the web site.  That scripts looks for the name of the program that is to be running based on the MAC address of the device and the provided executable name.  Basically the name of the program can be over loaded based on the content in the php table.   The executable name will be used by the script to get the timestamp of the file that is located on the web server.  this is compared to a time stamp that is collected by the web server of the last time it sent content to the device.  if the timestamp of the last time content was sent is newer then the timestamp on the uploaded image the webserver returns an indication that the caller should just run what they have.  if the oposite is turn the web server sends and indication that the latest image should be downloaded.

At this point the device should be running the latest version of the code.

It will then connect to the MQTT bus. it will subscribe for MGMT messages.(ie. reboot and others) I think it needs to download the config information about what lights map to what pins and what the last state was.  Not sure how I want that to happen.  It might be best served by another MQTT function that will allow the information to be stored and organized.(8.8.2023) probably need to update this section as changes are made.

I'm thinking the default light would be the built in LED the last state would be loaded from the state service and the light could be controlled by the JMRI program.

So I guess this program is a light controller and if you want to run other lights they would need to be mapped in the "State Service".  the config would be loaded on startup and the last known state would be recovered.

If JMRI changes the state the mapping would used to update the content in the "State Service" so if the controller is restarted it would take care of the all this stuff.

