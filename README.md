# Spa_Hot_Tub_WiFi_Monitor
Monitor your spa hot tub (gecko or Balboa) over WiFi with iphone/android push notifications.


The Spa uses an RJ45 connector. 

![RJ45 connector](https://raw.githubusercontent.com/DJSures/Spa_Hot_Tub_WiFi_Monitor/main/Images/rj45.jpg)

Pin 1	Power	Vcc
Pin 2	Input	Light
Pin 3	Input	Pump 1
Pin 4	Power	GND
Pin 5	Output	Data (Display)
Pin 6	Output	Clock
Pin 7	Input	Pump 2
Pin 8	Input	Temperature

Connect the Heltec WiFi 32 to the Spa using these steps. Also configure the pushover.net account. Register a new application and paste the credentials in the file.

   1) Choose the wifi mode by uncomment AP_MODE or not
   2) Enter the SSID/PWD for either AP or Client mode
   5) Connect speaker to pin 26 and the other speaker wire to GND
   6) Get USER Key and API Token from https://pushover.net/api
   7) Edit the #defines below

#Challenges
A drunk monkey created the spa protocol and circuit on these things. The signal on the RJ45 is super noisy, and the whole protocol design really missed out on an opprotunity for being a lot smarter. Considering they use clock and data wires to send the display, they should have just used RX/TX for serial to have bi-directional communication. But, they didn't, so instead I had to reverse engineer their mess. You'll notice in my code that I sample the signal 100-200 times before validating the temperature or display. This is because for what ever reason the data bits will sometimes not be set. So, to trust the data and interpret the temperature, I have to sample the packet 200 times. UGH!

You'll notice a number of comical hacks in my code because of their poor understanding of technology when the first spa packs were created. Rand is over :)
