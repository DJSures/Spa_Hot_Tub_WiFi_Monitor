# Spa_Hot_Tub_WiFi_Monitor
Monitor your spa hot tub (gecko or Balboa) over WiFi with iphone/android push notifications. I used a Heltec WiFi 32 because it has a nice little display and they're super compact. 

![Push notification example](https://github.com/DJSures/Spa_Hot_Tub_WiFi_Monitor/blob/main/Images/IMG_7088.PNG?raw=true)

![Operating in 3d printed case](https://github.com/DJSures/Spa_Hot_Tub_WiFi_Monitor/blob/main/Images/IMG_7089.jpg?raw=true)

# Connecting

The Spa uses an RJ45 connector. I used a RJ45 splitter to connect both the top controls and this Heltec WiFi 32 monitor program thingy that I made.

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
   3) Connect GND from the Spa (Pin #4) to the Heltec GND (they need to have common gnd)
   4) Connect Spa Data (Pin #5) to Heltech pin #12
   5) Connect Spa Clock (Pin #6) to Heltech pin #14
   6) Get USER Key and API Token from https://pushover.net/api
   7) Edit the #defines below

# How Does It Work?

I created this to monitor the temperature and heater activity of my hot tub during the winter months. Up here in Canada, things freeze in the winter. If the power went out or GFI tripped then I want to know if my hot tub needs attention. You could put the Heltec in the spa and power it off USB, but I ran a 50ft RJ45 ethernet cable and put the Heltec in my house. This way I can update the firmware and see the display. 

Anyway, so this little project will send a push notification to your phone every hour with the status of the hot tub temperature, heater and various other stats. The neat thing is you can see when the heater turns ON and OFF, and how long it was active for. Your phone will receive a push notification for the status change of the heater. That way, you can monitor how effecient the hot tub is at different temperatures when not in use. 

# Rant

A drunk monkey created the spa protocol and circuit on these things. The signal on the RJ45 is super noisy, and the whole protocol design really missed out on an opprotunity for being a lot smarter. Considering they use clock and data wires to send the display, they should have just used RX/TX for serial to have bi-directional communication. But, they didn't, so instead I had to reverse engineer their mess. You'll notice in my code that I sample the signal 100-200 times before validating the temperature or display. This is because for what ever reason the data bits will sometimes not be set. So, to trust the data and interpret the temperature, I have to sample the packet 200 times. UGH!

You'll notice a number of comical hacks in my code because of their poor understanding of technology when the first spa packs were created. Rand is over :)
