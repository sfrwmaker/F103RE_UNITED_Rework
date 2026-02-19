# F103RE_UNITED_Rework
The soldering and rework station managing T12, JBC C245 tips and Hot Air Gun
You can flash the F103RE_UNITED_Rework.hex directly to MCU or compile the entire project
The following directories should be market a 'included to project':
  FatFS
  JSON_PARSER
  TFT
  W25Qxx

Detailed instructions can be found on hackster.io site, https://www.hackster.io/sfrwmaker/united-soldering-and-rework-station-b4ad4f

Revision history

2025 MAR 14 v.1.01
  * Added erasing flash procedure allowing completely format the data store and check it
  * Fixed bug in adjusting the display brightness

2025 JUN 16 v.1.02
  * Changed approach to supply power to the Hot Air Gun
  * Changed PID parameters of the Hot Air Gun
  * Changed method to cooling down the Hot Air Gun
  * Changed the maximum Hot Air Gun temperature to 550 degrees Celsius
  * Fixed minor bugs

2025 NOV 12 v.1.03
  * Fixed flash write error issue when you are trying to modify your active tip list. 
  * Updated the manual calibration procedure. You can start calibrate your soldering tip from lower temperatures and the controller would approximate the calibration data for higher tepmperature reference points to simplify calibration process.
  * You can manage the Hot Air Gun speed in debug mode.
  * Added support for Hot Air Gun with fan running 12 volts. You can select your fan voltage version in Hot Air Gum menu (12 or 24 volts)
  * Added default ambient temperature paramater in menu setup. You can set the value of your ambient temperature via parameters menu. The Hakko T12 handle has termistor to check the ambient, temperature but JBC handle has no one. The default ambient temperature used when JBC iron is in use.

2025 FEB 19 v.1.04
  * Changed approach for reading analog signals, especcially the Hot Air Gun temperature, now the Gun is working more reliably
  * Changed the Hot Air Gun PID parameters
  * Added configuration parameter to manage the rotary encoder mode while the Hot Air Gun is working. Now it is possible to choose the default encoder mode: change the preset temperature or change the fan speed
