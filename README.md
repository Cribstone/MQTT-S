MQTT-S
======
  MQTT-S Client over XBee  (running on linux, Arduino and mbed)  
  
  Linux Client PUBLISH  ON and OFF message with topic dev/indicator.  
  Arduino Client SUBSCRIBE the topic dev/indicator and receives 'ON' and 'OFF' data  
  to trun on and off the indicator of Arduino.  
  http://www.youtube.com/watch?v=INa5YznfR-8&feature=youtu.be    
  
  ![outlook](https://github.com/TomoakiYAMAGUCHI/MQTT-S-Gateway/blob/master/TomyGateway/documents/MQTT-S_outlook.PNG?raw=true)

Supported functions
-------------------

*  QOS Level 0 and 1
*  Automatic SEARCHGW, GWINFO
*  Automatic CONNECT, WILLTOPICREQ, WILLTOPIC, WILLMSGREQ, WILLMSG
*  Automatic PINGREQ, PINGRESP
*  Automatic CONNACK, REGISTER, REGACK, SUBACK, PUBACK, UNSUBACK
*  
*  SUBSCRIBE, PUBLISH, UNSUBSCRIBE, DISCONNECT

Implemented control flows:  
   Application program executes publish() function,   
   Message flow as berrow is conducted automaticaly.  


                 Client              Gateway               Broker
                    |                   |                    |      
       publish() -->| --- SERCHGW ----> |                    |  
                    | <-- GWINFO  ----- |                    |  
                    | --- CONNECT ----> |                    |  
                    | <--WILLTOPICREQ-- |                    |  
                    | --- WILLTOPIC --> |                    |  
                    | <-- WILLMSGREQ -- |                    |  
                    | --- WILLMSG ----> | ---- CONNECT ----> |(accepted)     
                    | <-- CONNACK ----- | <--- CONNACK ----- |   
                    | --- PUBLISH ----> |                    |  
                    | <-- PUBACK  ----- | (invalid TopicId)  |  
                    | --- REGISTER ---> |                    |  
                    | <-- REGACK  ----- |                    |  
                    | --- PUBLISH ----> | ---- PUBLISH ----> |(accepted)  
                    | <-- PUBACK  ----- | <---- PUBACK ----- |    
                    |                   |                    |    
                    //                  //                   //      
                    |                   |                    |          
     subscribe() -->| --- SUBSCRIBE --> | ---- SUBSCRIBE --> |     
     [set Callback] | <-- SUBACK ------ | <--- SUBACK ------ |    
                    |                   |                    |    
                    //                  //                   //    
                    |                   |                    |    
                    | <-- REGISTER ---- | <--- PUBLISH ----- |<-- PUBLISH  
    [exec Callback] | <-- PUBLISH  ---- |                    |  
                    | --- PUBACK   ---> | ---- PUBACK  ----> |--> PUBACK  
                    |                   |                    |  
                

Usage
------
####Minimum requirements
  Three XBee S2 devices,  a coordinator, a gateway and a client.
  mosquitto broker
  TomyGateway   ( in MQTT-S-Gateway Repository)

####1) Start Gateway  (see  TomoakiYAMAGUCHI/MQTT-S-Gateway)  
    
    $ TomyGateway 1 /dev/ttyUSB0  [Broker's IP Address]  [PortNo]   
  
####2) Start Client   (argument is a device which XBee dongle connected.)  
    
    $ TomyClient ClientID /dev/ttyUSB1  
  
  ClientID is string.  
  A client will connect to the Gateway and publish the message,  
  dev/indicator  on or dev/indicator off.  
  
####3) Arduino client  
  Save the binary code into Arduino.  
  Then press the reset button. 
  SUBSCRIBE dev/indicator to ON and OFF the indicator. 
  
  
XBee configurations
----------------------
  Serial interfacing  of Clients and gateway.  
  Coordinator is default setting.
  
    [BD] 0-7   Arduino Clients : 3 (9600bps)  Linux Clients : 5 (38400bps)
    [D7] 1  
    [D6] 0 or 1  
    [AP] 2

  Other values are defaults. Baudrate is used by  mqtts.begin(device, _baudrate_) or mqtts.begin(_baudrate_) function.   
  In case of LINUX, if you set D6 to 1, uncomment a line //#define XBEE_FLOWCTL_CRTSCTS in Mqtts_Defines.h
  

How to Build (Requiered source code list)
-----------
####1) Linux Client
*  mqttslib    
*  MqttsClientApp.cpp  

####2) Arduino Client
_Copy mqttslib into Aruino Librally directory._

_Copy this file into Aruino sketch directory._
*  MqttsClientFwApp.ino
  
####3) mbed Client
*  mqttslib    
*  MbedClientApp.cpp  

Module descriptions
-------------------  
####1) MqttsClientApp.cpp  
Client application sample which is used for debug.

  
####2) MqttsClientFwApp.ino
  MqttsClient sample application for Arduino. 
    
###Modules in mqttslib

####1) MqttsClient.cpp
  MQTT-S Client Engine class. This Class is used by  a application.  
  Usages are shown as follows.
  
    MqttsClient mqtts = MqttsClient();  // Declare the client object
    mqtts.begin(argv[1], B9600);        // argv[1] is a serial device for XBee. ex) /dev/ttyUSB0 
    mqtts.init("Node-02");              // Get XBee's address64, short address and set XBee Node ID, 
    mqtts.setQos(1);                    // set QOS level.  0 or 1
    mqtts.setWillTopic(willtopic);      // set WILLTOPIC.   
    mqtts.setWillMessage(willmsg);      // set WILLMSG  those are sent automatically. 
    mqtts.setKeepAlive(60000);          // PINGREQ interval time

    mqtts.subscribe(topic, callback);   // Execute the callback, when the subscribed topic's data is published. 
    mqtts.publish(topic, payload, payload_length); // publish the data, topic is converted into ID automatically.
    mqtts.publish(topic, MQString* payload);  
    mqtts.unsubscribe(topic);  
    mqtts.disconnect();

    
####2) MqttsClientAppFw4Arduino.cpp
  Application framework for Arduino.
  Interupt and  watch dog timer are supported.
      
####3) MQTTS.cpp 
  MQTT-S messages classes and some classes for client and Gateway.
    
####4) ZBeeStack.cpp
  XBee control classes for MQTT-S
    
####5) Mqtts_Defines.h
  Default setting is Arduino.  (Both systems are comented out)  
  select the system and uncoment it.
    
    //#define LINUX 
    //#define MBED
    
  
  Flags for debug are follows:
  
    #define MQTTS_DEBUG     // show MQTT-S events.     
    #define XBEE_DEBUG // show serial I/O transactions. 
  


   

  
  
###Contact


* Author:    Tomoaki YAMAGUCHI
* Email:     tomoaki@tomy-tech.com
