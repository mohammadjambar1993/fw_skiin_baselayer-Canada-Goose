.. todo:: how should we reference ``Skiin System``?
.. todo:: link to hardware specs
.. include:: _names.rst

.. _lbl_overview:

Skiin Baselayer Overview
#####################################################

Hardware
*************************************************
The Skiin Baselayer is a textile system that can be heated for keeping warm or other treatment purpose. The system is powered by portable battery and includes heating control unit(referred as module or POD in this document) and various textiles with heating elements embedded. The POD features a microcontroller that controls up to five heating channels that are connected to the textile heating elements. There are thermistors embedded in the textile to sense temperature of heating elements and the pod processes the sensing data to get temperature and sends it to the App in a smart phone along with other data like heating elements resistance etc through BLE. 

Microcontroller
==================================================
The microcontroller used is the Nordic |nrf52| and the most relevant features for Skiin are the following:

    * ARM Cortex M4F Core operating at 64MHz
    * 512 kilobytes of code memory (Flash)
    * 64 kilobytes of volatile memory (:term:`RAM`)
    * Bluetooth Low Energy (BLE) version 4.2 
    * Two SPI ports operating independently at 8MHz
    * One I2C port operating at 400 KHz
    * Watchdog timer running from an independent clock source
    * 25 general purpose inputs and outputs digital pins (gpio)   

Firmware
*************************************************
The Baselayer firmware is responsible to control the hardware and read the external sensors. It resides in a microcontroller and processes the temperature sensing data ,heating elements resistance and controls heating channels with different heating active time and heating voltage. These processed data along with the heating status will be sent to the Skinn App running on a smart phone. The App can send heating commands to the pod to control heating process or the pod can work alone to control the heating.

    
Main Features
==================================================
The main firmware fatures are listed below:

    * Process data sensed from one to four thermistors to get temperature of the heating elements; Read current and voltage value from five current sensors through I2C for heating channels and calculate resistance of the heating elements
    * Communicate with PD(Power Delivery) controller through I2C to get power bank capability and exchange data with a BLE Master (smartphone) so that the App can choose heating voltage
    * Communicate with three axis accelerometer through SPI to detect tapping events that are used to toggle the heating channel on/off or other functions
    * Implements a BLE Peripheral role to exchange data with a BLE Master (smartphone)
    * Controls an :term:`RGB` LED to show the POD state (advertising, connected, error, etc)
    * Provides a way to update itself to a new firmware version
    * Controls an external power bank through PD controller to drive heating channels
    * Diagnoses itself and sends internal information to the phone for remote diagnostics
    * Reads battery level
    * Detect short circuit failure and shut down heating channels to protect the system
   

Communication Interface
==================================================
The firmware uses a Bluetooth Low Energy (BLE) radio to receive and send data to/from a smartphone. It implements a BLE Peripheral role and accepts connections from masters that have valid encryption keys.


BLE Security
===================================================
The communication between the POD and the smartphone is encrypted to guarantee the data privacy and only BLE masters that have the encryption key can start a connection with the POD.







         

   
