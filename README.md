# Camber Display

An Esp32 script that grabs data through MQTT channels and prints it by using the FT chip.

A MQTT server is required to serve data to the device. A Raspberry Pi comes in handy for a server. You can check out this python script for simulating and serving data using MQTT.

If a previous server setting isn't stored, device will put itself into a http server mode, serving a page where you can see wifi devices available and select the server device then enter the password to make the connection.

The server mac id & password will be stored in device memory until a reset.

Data wanted to be printed can be selected by sending an array to a configuration channel using MQTT. Then the device will subscribe itself to those channels and receive the data.

