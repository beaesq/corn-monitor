# corn-monitor
Arduino temp/rel humidity/power outage monitor w/ SMS alerts for seed cold storage.

Uses 6 DHT22 sensors to read temperature and relative humidity. Logs readings w/ date & time on an SD card. A 5V power supply is used to detect power outages. 

Sends SMS alerts for power outages. Sends SMS reports of temp & humidity readings every 1/2/3 days in 60/90/120 minute intervals. Can receive SMS commands to send temp & humidity report or check current SMS balance.

An LCD and 4 push buttons are used to display the current date/time, average temp & humidity, and individual sensor readings.

**Notes:**
  - SMS balance check is hard-coded for SMART haha
  - requires settings txt file in the sd card and dht22 & GSM shield libraries
  - profiles not implemented correctly
  - needs clean-up, more documentation, and more testing
