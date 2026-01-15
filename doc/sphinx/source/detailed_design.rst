.. todo:: [requirements] link hardware requirements to all requirements here
.. todo:: [requirements] document explaining the BLE protocol and firmware update
.. todo:: [requirements] put a link to mechanical drawing. See requirement 14
.. include:: _names.rst

.. _lbl_fw_detailed_design:

Firmware Detailed Design
#####################################################

    The Firmware Requirement Specification (FWRS) describes the functional and non-functional requirements for the Skiin Baselayer Firmware. It also tries to identify common risks associated with each of the requirements.
  

.. _lbl_fdd0001:

FWR0001 - Requirements List
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _lbl_fdd0001.1:

FWR0001.1 - Garment ID
**********************************************
The POD should be configured to provide up to five heating channels(A,B,C,D and E) with different heating voltage to drive different textiles. Garment ID will distiguish these textile types and before the ID is decided, no heating commands can be sent and executed. The firmware will determine the ID based on the :download:`SKIIN Heat Form Factors Document <docs/SKIIN Heat Form Factors - Garment IDs.pdf>`.  
The garment ID list:  
  
    *	0x00: unknown
    *	0x01: TOP_HANDS 
    *	0x02: LEG_THIGHS 
    *	0x03: TOP_TORSO
    *	0x04: TOP_GLOVES
    *	0x05: LEG_SOCKS

.. _lbl_fdd0001.1.1:

FWR0001.1.1 - ID Determine Process
==============================================
At start-up, the firmware will turn on the heating channel C every 10 seconds for about 10ms to measure the resistance of the channel, meanwhile it will process the data from the thermistor T0 to decide if the thermistor is installed. Combined these two parameters, the garment ID will be determined. See: SKIIN Heat Form Factors Document for detail.
After the ID is determined, heating function can be executed. When the heating is off, the firmware will also check to see if the ID is changed in the same process above.

.. _lbl_fdd0001.1.2:

FWR0001.1.2 - Notification For ID detection 
==============================================
When the ID is changed, the firmware will put the ID in the :ref:`lbl_char_modinfo` that is availible for the App to read. Meanwhile it will generate and send a notification in the same characteristic to the App if the BLE connection is established.  

.. _lbl_fdd0001.2:

FWR0001.2 - Firmware Versions
**********************************************
Firmware versions will be set in the :ref:`lbl_char_modinfo`, following the Semantic Versioning: major.minor.patch, where major and minor are 1 byte and patch is 2 bytes.

.. _lbl_fdd0001.3:

FWR0001.3 - Hardware  Version
**********************************************
Hardware version will be set in the :ref:`lbl_char_modinfo`. One byte is used for the hardware version.

.. _lbl_fdd0001.4:

FWR0001.4 - Heating Control
**********************************************
The POD will be able to drive up to 5 heating channels. All channels will run periodically with same period and heating voltage, but different duty cycle(on time). The app will control the POD to drive heating channel with heating commands. 

.. _lbl_fdd0001.4.1:

FWR0001.4.1 - Heating Commands
==============================================
The heating command consists of command id for 'Heating_Settings' and heating parameters. The detail for the command format refers to the :ref:`lbl_char_command`. 

.. _lbl_fdd0001.4.2:

FWR0001.4.2 - Commands Response
==============================================
After each each command the firmware should return the command status using the :ref:`lbl_char_response`. If the app tries to control a heating channel that is shorted or when garment ID is not determined, the firmware will return an appropriate code in the :ref:`lbl_char_response` showing the error code.

.. _lbl_fdd0001.5:

FWR0001.5 - Battery Information
**********************************************
The firmware will communicate with PD controller to get battery capabilities, measure the battery votage level and then set these values in the :ref:`lbl_char_modinfo`. If the battery level is too low, a notification in the same characteristic will be generated and sent to the app.

.. _lbl_fdd0001.6:

FWR0001.6 - Heating Channel Error Indication
**********************************************
For each heating channel, the channel error status will be stored in the :ref:`lbl_char_heating_info1` and :ref:`lbl_char_heating_info2` . If the status changes, a notification in the same characteristic will be generated and sent to the app. 
The following error states shall be covered:

    *	Shorted heating element
    *	Open heating element 
    *	Inactive heating element 
    *	Active heating element
 
.. _lbl_fdd0001.7:

FWR0001.7 - Heating Channel Parameters Indication
***************************************************
For each heating channel, the channel parameters including active time, resistance, voltage and temperature will be stored in the :ref:`lbl_char_heating_info1` and :ref:`lbl_char_heating_info2`. If the parameters changed, a notification in the same characteristic will be generated and sent to the app. 

.. _lbl_fdd0001.8:

FWR0001.8 - Reset Module
**********************************************
The app can send the :ref:`lbl_char_command` with the command id for 'Reset Module' resets the POD. This command should be used only during development and will be deprecated in next releases

.. _lbl_fdd0001.9:

FWR0001.9 - POD Serial Number
**********************************************
Each POD will have a unique serial number and will be written in production line and cannot be erased. The serial number is advertised in BLE advertising data field and can be read with the :ref:`lbl_char_command` with the command id for 'Read Serial Number from UICR' 


.. _lbl_fdd0002:

FWR0002 - Characteristics Description
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**List of Characteristics**

    * :ref:`lbl_char_modinfo`
    * :ref:`lbl_char_command`
    * :ref:`lbl_char_response`
    * :ref:`lbl_char_heating_info1`
    * :ref:`lbl_char_heating_info2`
   

.. _lbl_char_modinfo:

Module Information Characteristic (UUID |uuid_modinfo|)
********************************************************
This characteristic is used to read the POD information. The access is read-only and notifications can be generated when a state changes. The following table shows the data format used by this characteristic. A *(N)* after the field name shows that a change in the field value generates a BLE notification.

+--------+----------+---------------------+--------------------------------------+
|  Byte  |  Length  |     Field Name      |              Description             |
|        |  (bytes) |                     |                                      |
+========+==========+=====================+======================================+
|   0    |     1    |  Hardware Version   |           Hardware Version           |
+--------+----------+---------------------+--------------------------------------+
|   1    |     1    |        Major        |                                      |
+--------+----------+---------------------+                                      +
|   2    |     1    |        Minor        |           Firmware Version           |
+--------+----------+---------------------+                                      +
|  3..4  |     2    |        Patch        |                                      |
+--------+----------+---------------------+--------------------------------------+
|   5    |     1    |  Garment ID (N)     |       Identify the garment type      |
+--------+----------+---------------------+--------------------------------------+
|   6    |     1    |   Battery Range (N) |     Battery level in % (0..100)      |
+--------+----------+---------------------+--------------------------------------+
|   7    |     1    | Heating Period      | Global heating period (in seconds)   |
|        |          |                     | set with the Heating command         |
+--------+----------+---------------------+--------------------------------------+
|  8..14 |     7    | Battery Voltage     | Each byte represents a voltage level |
|        |          | Capabilities        | that the battery can provide. It is  |
|        |          |                     | given in volt multiplied by 10.      |
|        |          |                     | I.e. 50 is a 5v                      |
+--------+----------+---------------------+--------------------------------------+
| 15..19 |     5    |       Unused        |             Unused field             |
+--------+----------+---------------------+--------------------------------------+


.. _lbl_char_command:

Command Characteristic (UUID |uuid_command|)
********************************************************
This characteristic is used to send commands to the POD. A command is formed by a 2 bytes command id(opcode) followed by optional arguments that can use up to 18 bytes. This characteristic is write only.

+--------+----------+---------------------+--------------------------------------+
|  Byte  |  Length  |     Field Name      |              Description             |
|        |  (bytes) |                     |                                      |
+========+==========+=====================+======================================+
|  0..1  |     2    |       command id    |       Identifies the command         |
+--------+----------+---------------------+--------------------------------------+
| 2..19  |    18    |      Arguments      |          Command arguments           |
+--------+----------+---------------------+--------------------------------------+

.. note:: When a command is received the POD uses the :ref:`lbl_char_response` to inform the status to the requester.


**List of Available Commands**

    * :ref:`lbl_cmd_reset`
    * :ref:`lbl_cmd_set_heating`
    * :ref:`lbl_cmd_write_serial_num`
    * :ref:`lbl_cmd_read_serial_num`

.. _lbl_cmd_reset:

Reset (Opcode |opc_reset|)
===========================
This command resets the POD. It does not take any argument.

.. note:: 
    * this command does not return a response
    * the BLE connection will be closed


.. _lbl_cmd_set_heating:

Heat Settings (Opcode |opc_heat_set|)
=========================================
This command sets all channels heating time and battery level.
Seven bytes must be used as argument to set parameters for up to five channels. 
   
+--------+----------+---------------------+--------------------------------------+
|  Byte  |  Length  |     Field Name      |              Description             |
|        |  (bytes) |                     |                                      |
+========+==========+=====================+======================================+
|  0..4  |     5    |   Channel ON_Time   | Each byte sets heating on_time for   |
|        |          |                     | one of five channels                 |
+--------+----------+---------------------+--------------------------------------+
|   5    |     1    | Heating Period      | Set heating period for all channels  |
+--------+----------+---------------------+--------------------------------------+
|   6    |     1    | Heating Voltage     | Set heating voltage for all channels |
+--------+----------+---------------------+--------------------------------------+

.. _lbl_cmd_read_serial_num:

Read POD Serial Number (Opcode |opc_read_serial|)
===================================================
Reads the POD serial number.

.. note:: This command is used for development only and will be removed in future versions.


.. _lbl_cmd_write_serial_num:

Write POD Serial Number (Opcode |opc_write_serial|)
====================================================
This command receives and writes the POD serial number in flash memory. This command can only be issued once and trying to overwrite a serial number will generate an error. Writing the serial number can take up to one second and will reset the POD after the write operation is finished.

+--------+----------+---------------------+--------------------------------------+
|  Byte  |  Length  |     Field Name      |              Description             |
|        |  (bytes) |                     |                                      |
+========+==========+=====================+======================================+
|  0..1  |     2    |        Opcode       |              Command ID              |
+--------+----------+---------------------+--------------------------------------+
|  2..6  |     5    |        Serial       |             serial number            |
+--------+----------+---------------------+--------------------------------------+


.. _lbl_char_response:

Module Response Characteristic (UUID |uuid_response|)
********************************************************
This characteristic is used to return the status and data of a command issued with the :ref:`lbl_char_command`. It is formed by the command ID, command status and optionally the command data. This characteristic is read-only and sends data using BLE indication.

+--------+----------+---------------------+--------------------------------------+
|  Byte  |  Length  |     Field Name      |              Description             |
|        |  (bytes) |                     |                                      |
+========+==========+=====================+======================================+
|  0..1  |     2    |        Opcode       |              Command ID              |
+--------+----------+---------------------+--------------------------------------+
|   2    |     1    |        Status       |  0x00: successful command execution  |
|        |          |                     +--------------------------------------+
|        |          |                     |  0x01: invalid opcode                |
|        |          |                     +--------------------------------------+
|        |          |                     |  0x02: invalid arguments length      |
|        |          |                     +--------------------------------------+
|        |          |                     |  0x03: invalid command               |
|        |          |                     +--------------------------------------+
|        |          |                     |  0x04: invalid argument              |
|        |          |                     +--------------------------------------+
|        |          |                     |  0x05: command error                 |
+--------+----------+---------------------+--------------------------------------+
|  3..19 |     17   |        Data         |             Returned data            |
+--------+----------+---------------------+--------------------------------------+


.. _lbl_char_heating_info1:

Heating Channels Information Characteristic (UUID |uuid_heatinfo1|)
*********************************************************************
This characteristic is used to store heating parameters and status for channels A-C . The access is read-only and notifications can be generated when a state changes. The following table shows the data format used by this characteristic. A *(N)* after the field name shows that a change in the field value generates a BLE notification.

+-----+--------+----------+---------------------+--------------------------------------+
|  Ch |  Byte  |  Length  |     Field Name      |              Description             |
|     |        |  (bytes) |                     |                                      |
+=====+========+==========+=====================+======================================+
|     |   0    |     1    |       ON_Time(N)    |  Channel heating on_time             |
|     +--------+----------+---------------------+--------------------------------------+
|     |   1    |     1    |       Status(N)     |  Channel status for short,open etc   |
| A   +--------+----------+---------------------+--------------------------------------+  
|     |   2    |     1    |       Votage        | Channel heating votage in volt x10   |
|     +--------+----------+---------------------+--------------------------------------+                                      
|     |   3    |     1    |     Resistance(N)   | Channel heating resistance in ohm    |
|     +--------+----------+---------------------+--------------------------------------+
|     | 4..5   |     2    |   Temperature (N)   | Channel temperature in Celsius x100  |
+-----+--------+----------+---------------------+--------------------------------------+
|     |   6    |     1    |       ON_Time(N)    |  Channel heating on_time             |
|     +--------+----------+---------------------+--------------------------------------+
|     |   7    |     1    |       Status(N)     |  Channel status for short,open etc   |
| B   +--------+----------+---------------------+--------------------------------------+                                      
|     |   8    |     1    |       Votage        | Channel heating votage in volt x10   |
|     +--------+----------+---------------------+--------------------------------------+                                      
|     |   9    |     1    |     Resistance(N)   | Channel heating resistance in ohm    |
|     +--------+----------+---------------------+--------------------------------------+
|     | 10..11 |     2    |   Temperature (N)   | Channel temperature in Celsius x100  |
+-----+--------+----------+---------------------+--------------------------------------+
|     |   12   |     1    |       ON_Time(N)    |  Channel heating on_time             |
|     +--------+----------+---------------------+--------------------------------------+
|     |   13   |     1    |       Status(N)     |  Channel status for short,open etc   |
| C   +--------+----------+---------------------+--------------------------------------+                                      
|     |   14   |     1    |       Votage        | Channel heating votage in volt x10   |
|     +--------+----------+---------------------+--------------------------------------+                                      
|     |   15   |     1    |     Resistance(N)   | Channel heating resistance in ohm    |
|     +--------+----------+---------------------+--------------------------------------+
|     | 16..17 |     2    |   Temperature (N)   | Channel temperature in Celsius x100  |
+-----+--------+----------+---------------------+--------------------------------------+


.. _lbl_char_heating_info2:

Heating Channels Information Characteristic (UUID |uuid_heatinfo2|)
*********************************************************************
This characteristic is used to store heating parameters and status for channels D-E . The access is read-only and notifications can be generated when a state changes. The following table shows the data format used by this characteristic. A *(N)* after the field name shows that a change in the field value generates a BLE notification.

+-----+--------+----------+---------------------+--------------------------------------+
|  Ch |  Byte  |  Length  |     Field Name      |              Description             |
|     |        |  (bytes) |                     |                                      |
+=====+========+==========+=====================+======================================+
|     |   0    |     1    |       ON_Time(N)    |  Channel heating on_time             |
|     +--------+----------+---------------------+--------------------------------------+
|     |   1    |     1    |       Status(N)     |  Channel status for short,open etc   |
| D   +--------+----------+---------------------+--------------------------------------+  
|     |   2    |     1    |       Votage        | Channel heating votage in volt x10   |
|     +--------+----------+---------------------+--------------------------------------+                                      
|     |   3    |     1    |     Resistance(N)   | Channel heating resistance in ohm    |
|     +--------+----------+---------------------+--------------------------------------+
|     | 4..5   |     2    |   Temperature (N)   | Channel temperature in Celsius x100  |
+-----+--------+----------+---------------------+--------------------------------------+
|     |   6    |     1    |       ON_Time(N)    |  Channel heating on_time             |
|     +--------+----------+---------------------+--------------------------------------+
|     |   7    |     1    |       Status(N)     |  Channel status for short,open etc   |
| E   +--------+----------+---------------------+--------------------------------------+                                      
|     |   8    |     1    |       Votage        | Channel heating votage in volt x10   |
|     +--------+----------+---------------------+--------------------------------------+                                      
|     |   9    |     1    |     Resistance(N)   | Channel heating resistance in ohm    |
|     +--------+----------+---------------------+--------------------------------------+
|     | 10..11 |     2    |   Temperature (N)   | Channel temperature in Celsius x100  |
+-----+--------+----------+---------------------+--------------------------------------+
