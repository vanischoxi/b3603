# Serial Protocol

This is a description of the serial protocol for this alternative firmware.

## Configuration

The serial works at 38400 8N1, the low speed is intended to allow MCU to handle
the flow without interfering too much with the rest of the work.

## Startup

The controller will no send welcome message

## Commands

All commands are line-feed or carriage-return terminated, if the input buffer
is filled with no LF or CR character the entire line is dumped and a message is
sent on the output to indicate this failure.

### System Configuration Query

* Send: "SYSTEM"
* Receive: "MODEL: <model>\r\nVERSION: <version>\r\nNAME: <name>\r\nONSTARTUP: <ON/OFF>\r\nAUTOCOMMIT: <YES/NO>\r\n"

Get the system information: model, version, name, auto-on on startup and auto commit.

### Commit configuration

* Send: "COMMIT"
* Receive: "DONE\r\n"

If auto commit is off this command will change the operating parameters according to the changes done since the last commit.
If auto commit is on, this command is not going to change anything.

### Auto-commit Set

* Send: "AUTOCOMMIT <YES/NO>"
* Receive: "AUTOMMIT: YES" or "AUTOCOMMIT: NO"

Set the auto commit setting.

### Name Set

* Send: "SNAME"
* Receive: "SNAME: <name>"

Set the name to what the user gave. The size is limited to 16 characters and
they must be printable characters.

### Calibration Values

* Send: "CALIBRATION"
* Receive: detailed on calibration, mostly useful to debug and comparison of units  
  CALIBRATE VIN ADC: 0006.6000/0000.0000
  CALIBRATE VOUT ADC: 0005.5120/0575.5613
  CALIBRATE COUT ADC: 0000.5156/0200.0000
  CALIBRATE VOUT PWM: 0000.1817/0117.9426
  CALIBRATE COUT PWM: 0001.9394/0160.0000


### Output Enable/Disable

* Send: "OUTPUT 0" or "OUTPUT 1"
* Receive: "OUTPUT: ON or "OUTPUT: OFF"

OUTPUT 0 disables the output and OUTPUT 1 enables the output.

### Voltage Set

* Send: "VOLTAGE XXXX"
* Receive: "VOLTAGE: SET X.XXX"

Set the maximum voltage level in mV.

### Current Set

* Send: "CURRENT XXXX"
* Receive: "CURRENT: SET X.XXX"

Set the maximum current level in mA.

### Over voltage protection (not checked, task)

* Send: "VSHUTDOWN X.XXXX" or "VSHUTDOWN 0"
* Receive: "VSHUTDOWN: X.XXXX" or "VSHUTDOWN: DISABLED"

When a VSHUTDOWN is set and reached the unit will turn off the output to avoid
an over-voltage situation. This would be used in a constant current situation
by setting the shutdown voltage lower than the max controlled voltage and let
the microcontroller shutdown the entire output if the limit is reached.

### Over current protection (not checked, task)

* Send: "CSHUTDOWN 1" or "CSHUTDOWN 0"
* Receive: "CSHUTDOWN: ENABLED" or "CSHUTDOWN: DISABLED"

When a CSHUTDOWN is set and reached the unit will shutdown, current overload is
considered when we reach the maximum current and the voltage drops by 10%
already to handle the current control, at that point it is assumed the load is
in short and the output is shutdown.

### Query configuration

* Send: "CONFIG"
* Receive: "CONFIG:\r\nOUTPUT: <Output>\r\nVSET: <Voutmax>\r\nCSET: <Ioutmax>"

Report all the config variables:

* Output -- Output enabled "ON" or disabled "OFF"
* Voutmax -- Voltage output maximum
* Ioutmax -- Current output max as set

### Status Report

* Send: "STATUS"
* Receive: "STATUS:\r\nOUTPUT: <Output>\r\nVIN: <Vin>\r\nVOUT: <Vout>\r\nCOUT: <Iout>\r\nCONSTANT: <CCCV>\r\n"

Reports all state variables:

* Output -- Output enabled "ON" or disabled "OFF"
* Vin -- Voltage Input to the unit
* Vout -- Actual voltage output
* Iout -- Actual current output
* CCCV -- "CURRENT" if we are in constant current, "VOLTAGE" if we are in constant voltage


### Limits Report

* Send: "LIMITS"
* Receive: "LIMITS:\rVMIN: <VMin>\r\nVMAX: <VMax>\r\nVSTEP: <VStep>\r\nCMIN: <IMin>\r\nCMAX: <CMax>\r\nCSTEP: <CStep>\r\n"

Reports all state variables:

* VMIN -- Min Voltage output
* VMAX -- Max Voltage output
* VSTEP -- Voltage steps
* CMIN -- Min Current output
* CMAX -- Max Current output
* CSTEP -- Current steps

### RStatus Report

* Send: RSTAUTS"
* Receive: "RSTATUS:\rVIN ADC: <VINADC>\r\nVOUT ADC: <VOUTADC>\r\nVOUT ADC: <VOUTADC>\r\nCOUT ADC: <COUTADC>"

### Save configuration EEPROM

* Send: SAVE"
* Receive: "SAVED\r\n"

### Load configuration

* Send: LOAD"



## Missing features

* Calibration only via Phyton script
* Watchdog, can we output something on the serial?
* Internal notification logic, monitor the output values and notify immediately
  on non-trivial change (i.e. apply hysteresis so minutiae errors are not
  noisy)
