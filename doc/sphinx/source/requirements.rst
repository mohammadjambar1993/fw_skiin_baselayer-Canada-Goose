.. include:: _names.rst

.. _lbl_fw_requirements:

Firmware Requirements
#####################################################

    The Firmware Requirement Specification (FWRS) describes the functional and non-functional requirements for the Skiin Baselayer Firmware. 

    **Input**
        
        * User Requirements Specification
        * Product Requirements Specification
  

.. _lbl_fwr0001:

FWR0001 - Heating Control and Monitoring
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The firmware shall control up to four heating elements; each heating element is attached to one of the POD's power outputs. 

.. _lbl_fwr0001.1:

FWR0001.1 - Output Control
**********************************************

The firmware shall provide a way to control the amount of energy applied to each output/heating element. The output power level is a function of the electric potential from the power bank, and the duty cycle individually applied to each heating element. 


.. _lbl_fwr0001.2:

FWR0001.2 - Power Bank Control
**********************************************

The firmware shall control the power bank and select the correct electrical potential value (VCC) to power the heating elements.


.. _lbl_fwr0001.3:

FWR0001.3 - Temperature Reading
**********************************************

The firmware shall read the resistive temperature sensor associated with each heating element, transforming resistance and voltage readings into Celsius. The temperature reading shall be available in the BLE protocol.


.. _lbl_fwr0001.4:

FWR0001.4 - Heating Element Voltage Reading
**********************************************

For each output, the firmware shall read the electric potential applied to a heating element. This information shall be available in the BLE protocol.


.. _lbl_fwr0001.5:

FWR0001.5 - Heating Element Resistance Reading
*************************************************

The firmware shall read the electric resistance of the heating element. This information shall be available in the BLE protocol.


.. _lbl_fwr0001.6:

FWR0001.6 - Heating Element Current Reading
*************************************************

The firmware shall read the electric current flowing through each heating element.


.. _lbl_fwr0001.7:

FWR0001.7 - Output Current Protection
*************************************************

The firmware shall monitor the current flow on each heating element and disable the heater output if the current is above the acceptable limit.


.. _lbl_fwr0001.8:

FWR0001.8 - Heating Element Temperature Protection
***************************************************

The firmware shall monitor the heating element temperature and disable the heater output if the temperature is higher than 40 degrees Celsius.


.. _lbl_fwr0001.9:

FWR0001.9 - Heating Element Time Protection
***************************************************

The firmware shall monitor the heating element and turn off the heater if it is active for a long period of time.


.. _lbl_fwr0001.10:

FWR0001.10 - Garment Type Detection
****************************************************

The firmware shall detect the garment that is being used by measuring the resistance of the heating elements and thermistors. Different garments have a unique combination of heating elements and thermistors.
 

.. _lbl_fwr0001.11:

FWR0001.11 - Closed Loop Temperature Control
****************************************************

The firmware shall implement a temperature control algorithm to keep the heating element temperature constant when there are variations due to the environment temperature.


.. _lbl_fwr0002:

FWR0002 - Temperature Profiles
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The firmware shall support different temperature profiles. Each profile is a combination of temperature and activation time and can be programmed using the Skiin app. The profiles should be stored in the flash memory.


.. _lbl_fwr0003:

FWR0003 - Manual Profile Control
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The firmware shall provide a way for the user to control the temperature settings when a phone is not available.
The firmware shall read the :term:`IMU` sensor to detect taps. Three temperature profiles must be available with tapping: low, medium and high temperature.


.. _lbl_fwr0004:

FWR0004 - Battery Capacity
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The firmware shall read the battery remaining charge and make this information available in the BLE protocol.


.. _lbl_fwr0005:

FWR0005 - Sleep Mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Firmware shall enter sleep mode when the POD is inactive for a period of time.


.. _lbl_fwr0006:

FWR0006 - Firmware Secure Update
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The firmware shall provide a secure way to update itself when the module is in the field.


.. _lbl_fwr0007:

FWR0007 - Serial Number Storage
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The POD shall have a unique serial number and the firmware shall provide a way to store this number before the POD is shipped to the customer.


.. _lbl_fwr0008:

FWR0008 - Indication LEDs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The firmware shall control three indication LEDs (RGB) to indicate internal status like connected, disconnected,
errors, etc.


.. _lbl_fwr00009:

FWR0009 - BLE Communication
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The firmware shall provide a communication interface using the :term:`BLE` Protocol Version 4.0. The firmware shall implement a BLE Peripheral role with proprietary services and characteristics that allow POD data to be accessed by a BLE Master. The communication between the peripheral and the master must be protected using encryption.


.. _lbl_fwr00009.1:

FWR0009.1 - BLE Advertising
******************************************
When BLE is disconnected, the module shall advertise its name and the Skiin Service UUID, allowing BLE Masters to scan the peripheral and initiate a connection. When advertising, the blue LED must blink rapidly. 


.. _lbl_fwr00009.2:

FWR0009.2 - BLE Connection
******************************************
When BLE is connected, the module shall stop advertising its name and the Skiin Service UUID, refusing all new BLE connection requests. When connected, the blue LED must blink twice each second. 


.. _lbl_fwr00009.3:

FWR0009.3 - BLE Communication Protocol
******************************************
The firmware shall implement a proprietary communication protocol in order transfer data to the smartphone.
The data transfer will involve sensors' data, commands, responses and diagnostics information.
The BLE Communication Protocol will be detailed in the Firmware Detailed Design Document.



FWR0009.3.1 - Module Information Characteristic
=================================================
The firmware shall maintain a BLE characteristic to make the following information available:

    * Firmware version
    * Hardware version        
    * Battery level
    * Battery output levels (Volts)
    * Garment Identification


FWR0009.3.2 - Command Characteristic
=================================================
The firmware shall maintain a BLE characteristic that can be used by the master to send commands to the POD. 
The following commands must be available:

    * Reset POD
    * Set Heating Settings
    * Write POD serial number
    * Read POD serial number

    
FWR0009.3.3 - Command Response Characteristic
=================================================
The firmware shall maintain a BLE characteristic that is used to send the status of a command.
The status code is used to identify if the command was successfully executed.


FWR0009.3.4 - Heating Channel Characteristic
=================================================
The firmware shall maintain a BLE characteristic that is used to publish the current Heating Channel parameters/measurements. The parameters available per channel are:

    * Heating time
    * Output channel status (active, inactive, shorted, opened)
    * Output voltage (Volts)
    * Heating element resistance (Ohms)
    * Heating element temperature (Celsius)


