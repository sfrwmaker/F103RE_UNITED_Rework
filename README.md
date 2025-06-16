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
