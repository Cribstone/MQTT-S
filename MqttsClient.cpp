/*
 * MqttsClient.cpp
 *
 *                    The MIT License (MIT)
 *
 *               Copyright (c) 2013, Tomoaki YAMAGUCHI
 *                       All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 *     Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *     Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation 
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * 
 *  Created on: 2013/06/17
 *      Author: Tomoaki YAMAGUCHI
 *     Version: 1.0.0
 *
 */
#ifdef ARDUINO
  #include <MQTTS_Defines.h>
#else
  #include "MQTTS_Defines.h"
#endif

#ifdef ARDUINO
  #include <SoftwareSerial.h>
  #include <MqttsClient.h>

  #if defined(DEBUG_ZBEESTACK) || defined(DEBUG_MQTTS)
        extern SoftwareSerial debug;
  #endif

#endif  /* ARDUINO */

#ifdef MBED
  #include "mbed.h"
  #include "MqttsClient.h"

  #if defined(DEBUG_ZBEESTACK))
        extern Serial debug;
  #endif
#endif  /* MBED */

#ifdef LINUX
  #include "MqttsClient.h"
  #include <stdio.h>
  #include <sys/time.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <unistd.h>
  #include <stdlib.h>
  #include <string.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <termios.h>
#endif /* LINUX */

using namespace std;

static MqttsClient*  theMqtts;

/*=====================================
        Class GatewayHandller
 ======================================*/
GatewayHandller::GatewayHandller(){
    _gwId = 0;
        _status = 0;
        _keepAliveDuration = 0;
        _advertiseDuration = 0;
        _addr16 = 0;
}

bool GatewayHandller::isConnected(){
    return (_status == MQTTS_GW_CONNECTED ? true : false);
}

bool GatewayHandller::isDisconnected(){
    return (_status == MQTTS_GW_DISCONNECTED ? true : false);
}

bool GatewayHandller::isSearching(){
    return (_status == MQTTS_GW_SEARCHING ? true : false);
}

bool GatewayHandller::isFound(){
    return (_status == MQTTS_GW_FOUND ? true : false);
}

bool GatewayHandller::isLost(){
    if ( _advertiseTimer.isTimeUp(_advertiseDuration)){
        _status = MQTTS_GW_LOST;
        return true;
    }else{
        return false;
    }
}

bool GatewayHandller::isInit(){
    return (_status == MQTTS_GW_INIT ? true : false);
}

bool GatewayHandller::isPingRequired(){
    return (_keepAliveTimer.isTimeUp(_keepAliveDuration) && isConnected());
}

XBeeAddress64* GatewayHandller::getAddress64(){
    return &_addr64;
}

uint16_t GatewayHandller::getAddress16(){
    return _addr16;
}

void GatewayHandller::setStatus(uint8_t status){
    _status = status;
}

void GatewayHandller::recvGwInfo(MqttsGwInfo* msg){
    if (_status == MQTTS_GW_LOST || _status == MQTTS_GW_INIT || _status == MQTTS_GW_SEARCHING){
        if (msg->getLength() == 3){
            _addr64 = theMqtts->getRxRemoteAddress64();
            _addr16 = theMqtts->getRxRemoteAddress16();
            _gwId = msg->getBody()[0];
            _status = MQTTS_GW_FOUND;
        }
    }
}

void GatewayHandller::recvPingResp(){
    _keepAliveTimer.start();
}

void GatewayHandller::setLastSendTime(){
    _keepAliveTimer.start();
}

void GatewayHandller::recvAdvertise(MqttsAdvertise* adv){
    if ( adv->getGwId() == _gwId){
        _advertiseTimer.start();
        _advertiseDuration = adv->getDuration() * 1.5;
        _gwId = adv->getGwId();
    }
    // ToDo  Update list of gateways.  elements are ZBNode
}

void GatewayHandller::setKeepAlive(long msec){
        _keepAliveDuration = msec;
}

long GatewayHandller::getKeepAlive(){
        return _keepAliveDuration;
}


/*===============================================================================

        Class MqttsClient

 ================================================================================*/
void ResponseHandler(ZBRxResponse* resp, int* returnCode){
        theMqtts->recieveMessageHandler(resp, returnCode);
}

MqttsClient::MqttsClient(){
    _zbee = new ZBeeStack();
    _sp = new SerialPort();
    _zbee->setSerialPort(_sp);
    _zbee->setRxHandler(ResponseHandler);
    _sendQ = new SendQue();
    _qos = 0;
    _duration = 0;
    _clientId = new MQString();
    _clientFlg = 0;
    _nRetry = 3;
    _tRetryMsec = 10000; // 10 sec
    _nRetryCnt = 0;
    _tRetry = 0;
    _willTopic = _willMessage = NULL;
    _gwHdl.setKeepAlive(MQTTS_DEFAULT_KEEPALIVE);
    _msgId = 0;
    _topics.allocate(MQTTS_MAX_TOPICS);
    theMqtts = this;
}

MqttsClient::~MqttsClient(){
  _sendQ->deleteAllRequest();
  delete _zbee;
  delete _sp;
}

#ifdef ARDUINO
void MqttsClient::begin(long baudrate){
        _sp->begin(baudrate);
}
#endif /* ARDUINO */

#ifdef MBED
void MqttsClient::begin(long baudrate){
            _sp->begin(baudrate);
    }
#endif /* MBED */

#ifdef LINUX
  void MqttsClient::begin(char* device, unsigned int bauderate){
      if( _sp->begin(device, bauderate) < 0){
      fprintf( stdout," Serialport open Error %s", device);
        exit(-1);
      }
  }
#endif  /* LINUX */




bool MqttsClient::init(const char* clientNameId){
    _clientId->copy(clientNameId);
        return _zbee->init(ZB_CLIENT,clientNameId);
}

Topics* MqttsClient::getTopics(){
    return &_topics;
}

void MqttsClient::setKeepAlive(long msec){
    _gwHdl.setKeepAlive(msec);
}

void MqttsClient::setWillTopic(MQString* topic){
    _willTopic = topic;
    _clientFlg |= MQTTS_FLAG_WILL;
}

void MqttsClient::setWillMessage(MQString* msg){
    _willMessage = msg;
    _clientFlg |= MQTTS_FLAG_WILL;
}

void MqttsClient::setQos(uint8_t level){
    if (level == 0){
            _clientFlg |= MQTTS_FLAG_QOS_0;
    }else if (level == 1){
            _clientFlg |= MQTTS_FLAG_QOS_1;
    }
    _qos = level;
}

void MqttsClient::setRetain(bool retain){
    _clientFlg |= MQTTS_FLAG_RETAIN;
}

void MqttsClient::setClean(bool clean){
    _clientFlg |= MQTTS_FLAG_CLEAN;
}

void MqttsClient::setRetryMax(uint8_t cnt){
    _nRetry = cnt;
}

XBeeAddress64& MqttsClient::getRxRemoteAddress64(){
    return _zbee->getRxRemoteAddress64();
}

uint16_t MqttsClient::getRxRemoteAddress16(){
    return _zbee->getRxRemoteAddress16();
}

MQString* MqttsClient::getClientId(){
    return _clientId;
}


uint16_t MqttsClient::getNextMsgId(){
    _msgId++;
    if (_msgId == 0){
        _msgId = 1;
    }
    return _msgId;
}

bool MqttsClient::isGwConnected(){
    return _gwHdl.isConnected();
}

void MqttsClient::clearMsgRequest(){
    _sendQ->deleteRequest(0);
}

uint8_t MqttsClient::getMsgRequestType(){
    if (_sendQ->getMessage(0)){
        return _sendQ->getMessage(0)->getType();
    }else{
        return 0;
    }
}
uint8_t MqttsClient::getMsgRequestStatus(){
    return _sendQ->getStatus(0);
}

uint8_t MqttsClient::getMsgRequestCount(){
  return _sendQ->getCount();
}

void MqttsClient::setMsgRequestStatus(uint8_t stat){
    _sendQ->setStatus(0,stat);
}

void MqttsClient::createTopic(MQString* topic, TopicCallback callback){
    _topics.addTopic(topic);
    _topics.setCallback(topic, callback);
}

void MqttsClient::delayTime(long maxTime){
#ifdef ARDUINO
    srand((unsigned)millis( ));
    uint32_t tm = rand() % maxTime;
#else
    srand((unsigned)time( NULL ));
    uint32_t tm = (rand() % maxTime) * 1000;
#endif
    XTimer delayTimer;
    delayTimer.start(tm);
    while(!delayTimer.isTimeUp()){
        _zbee->readPacket();
    }
}

void MqttsClient::copyMsg(MqttsMessage* msg, ZBRxResponse* recvMsg){
    memcpy(msg->getMsgBuff(), recvMsg->getData(), recvMsg->getData()[0]);
}

void MqttsClient::run(){
    while(true){
        int rc = execMsgRequest();
        if ((rc != MQTTS_ERR_NO_ERROR || getMsgRequestCount() != 0) &&
            getMsgRequestStatus() != MQTTS_MSG_REQUEST){
            clearMsgRequest();
        }else{
            break;
        }
    }
}

void MqttsClient::runConnect(){
    while(true){
        int rc = execMsgRequest();
        if (isGwConnected()){
            break;
        }else if ( rc != MQTTS_ERR_NO_ERROR){
            if ( getMsgRequestType() == MQTTS_TYPE_SEARCHGW){
                setMsgRequestStatus(MQTTS_MSG_REQUEST);
            }else{
                clearMsgRequest();
            }
        }
    }
}

void MqttsClient::runLoop(){
    while(true){
        int rc = execMsgRequest();
        if ((rc != MQTTS_ERR_NO_ERROR || getMsgRequestCount() != 0) &&
                getMsgRequestStatus() != MQTTS_MSG_REQUEST){
                clearMsgRequest();
        }
    }
}
/*========================================================
    Send a MQTT-S Message (add the send request)
==========================================================*/
int MqttsClient::requestSendMsg(MqttsMessage* mqttsMsgPtr){
    int index = _sendQ->addRequest((MqttsMessage*)mqttsMsgPtr);
    if ( index >= 0){
        if (_sendQ->getStatus(index) != MQTTS_MSG_RESEND_REQ){
            _sendQ->setStatus(index, MQTTS_MSG_REQUEST);
        }
        return MQTTS_ERR_NO_ERROR;
    }
    return MQTTS_ERR_CANNOT_ADD_REQUEST;
}

/*========================================================
  Send a MQTT-S Message (add to the top of the send request)
==========================================================*/
int MqttsClient::requestPrioritySendMsg(MqttsMessage* mqttsMsgPtr){
    if (_sendQ->addPriorityRequest((MqttsMessage*)mqttsMsgPtr)){
        return MQTTS_ERR_CANNOT_ADD_REQUEST;
    }else{
        if (_sendQ->getStatus(0) != MQTTS_MSG_RESEND_REQ){
            _sendQ->setStatus(0, MQTTS_MSG_REQUEST);
        }
        return MQTTS_ERR_NO_ERROR;
    }
}

/*=============================
 *   Send or Receive Message
 ==============================*/
int MqttsClient::execMsgRequest(){
    int rc;

    if (getMsgRequestStatus() == MQTTS_MSG_REQUEST || getMsgRequestStatus() == MQTTS_MSG_RESEND_REQ){
        if (_gwHdl.isLost() || _gwHdl.isInit()){
            if ( searchGw(ZB_BROADCAST_RADIUS_MAX_HOPS) == MQTTS_ERR_CANNOT_ADD_REQUEST){
                return MQTTS_ERR_CANNOT_ADD_REQUEST;
            }else{
                _gwHdl.setStatus(MQTTS_GW_SEARCHING);
            }
        }
        if (_gwHdl.isSearching()){
            /*------------ Send SEARCHGW --------------*/
            if (getMsgRequestType() == MQTTS_TYPE_SEARCHGW){
                delayTime(MQTTS_TIME_SEARCHGW);
                if (getMsgRequestType() == MQTTS_TYPE_SEARCHGW){
                    rc = broadcast(MQTTS_TIME_SEARCHGW);
                    if ( rc != MQTTS_ERR_NO_ERROR){
                        return rc;
                    }
                }
            }
        }
        if (_gwHdl.isDisconnected() || _gwHdl.isFound()){
            /*-----------  Send CONNECT,WILLTOPIC, WILLMSG ----------*/
           if (getMsgRequestType() == MQTTS_TYPE_CONNECT ||
               getMsgRequestType() == MQTTS_TYPE_WILLTOPIC ||
               getMsgRequestType() == MQTTS_TYPE_WILLMSG){
               rc = unicast(MQTTS_TIME_RESPONCE);
               if (rc == 0 && _qos == 0){
                   _gwHdl.setStatus(MQTTS_GW_CONNECTED);
               }
               return rc;
           }
        }
        if (_gwHdl.isConnected()){
            /*-------- Send PUBLISH,SUBSCRIBE,REGISTER,DISCONNECT,PUBACK -------*/
            return unicast(MQTTS_TIME_RESPONCE);
        }
        return MQTTS_ERR_NOT_CONNECTED;
    }else{
        if (_gwHdl.isPingRequired()){
            /*-------- Send PINGREQ -----------*/
            pingReq(_clientId);
            if (unicast(MQTTS_TIME_RESPONCE)){
                _gwHdl.setStatus(MQTTS_GW_LOST);
                return MQTTS_ERR_PINGRESP_TIMEOUT;
            }
        }
        _zbee->readPacket();  //  Receive MQTT-S Message
        return MQTTS_ERR_NO_ERROR;
    }
}

/*------------------------------------
 *   Broad cast the MQTT-S Message
 -------------------------------------*/
int MqttsClient::broadcast(uint16_t packetReadTimeout){
    int retry = 0;
    while(retry < _nRetry){
        _zbee->bcastData(_sendQ->getMessage(0)->getMsgBuff(),
                         _sendQ->getMessage(0)->getLength());
        _respTimer.start(packetReadTimeout);

        while(!_respTimer.isTimeUp()){
           if ((_qos == 0 && getMsgRequestType() != MQTTS_TYPE_SEARCHGW)|| getMsgRequestStatus() == MQTTS_MSG_COMPLETE){
               clearMsgRequest();
               return MQTTS_ERR_NO_ERROR;
           }else if (getMsgRequestStatus() == MQTTS_MSG_REQUEST){
               setMsgRequestStatus(MQTTS_MSG_WAIT_ACK);
           }
           _zbee->readResp();
        }
        setMsgRequestStatus(MQTTS_MSG_REQUEST);
        retry++;
    }
   return MQTTS_ERR_RETRY_OVER;
}

/*------------------------------------
 *   Unicast the MQTT-S Message
 -------------------------------------*/
int MqttsClient::unicast(uint16_t packetReadTimeout){
    int retry = 0;
    while(retry < _nRetry){
        _zbee->sendData(_gwHdl.getAddress64(), _gwHdl.getAddress16(),
                        _sendQ->getMessage(0)->getMsgBuff(),
                        _sendQ->getMessage(0)->getLength(), 0);
        _respTimer.start(packetReadTimeout);

        while(!_respTimer.isTimeUp()){
            if ((_qos == 0 && getMsgRequestType() != MQTTS_TYPE_PINGREQ ) ||
                              getMsgRequestType() == MQTTS_TYPE_PUBACK   ||
                              getMsgRequestStatus() == MQTTS_MSG_COMPLETE ){
                clearMsgRequest();
                _gwHdl.setLastSendTime();
                return MQTTS_ERR_NO_ERROR;

            }else if (getMsgRequestStatus() == MQTTS_MSG_REJECTED){
                return MQTTS_ERR_REJECTED;

            }else if (getMsgRequestStatus() == MQTTS_MSG_RESEND_REQ){
                _zbee->sendData(_gwHdl.getAddress64(), _gwHdl.getAddress16(),
                                _sendQ->getMessage(0)->getMsgBuff(),
                                _sendQ->getMessage(0)->getLength(), 0);

                setMsgRequestStatus(MQTTS_MSG_WAIT_ACK);

            }else if (getMsgRequestStatus() == MQTTS_MSG_REQUEST){
                setMsgRequestStatus(MQTTS_MSG_WAIT_ACK);
            }
            _zbee->readResp();
        }
        setMsgRequestStatus(MQTTS_MSG_REQUEST);
        retry++;
    }
    return MQTTS_ERR_RETRY_OVER;
}

/*========================================
 *   Create & send the MQTT-S Messages
 =========================================*/

/*--------- CONNECT ------*/
int MqttsClient::connect(){
    MqttsConnect mqttsMsg = MqttsConnect(_clientId);
    mqttsMsg.setDuration(_gwHdl.getKeepAlive());
    mqttsMsg.setFlags(_clientFlg);
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- PUBLISH ------*/
int MqttsClient::publish(MQString* topic, const char* data, int dataLength){
    uint16_t topicId = _topics.getTopicId(topic);
    if (topicId){
        MqttsPublish mqttsMsg = MqttsPublish();
        mqttsMsg.setFlags(_clientFlg | MQTTS_TOPIC_TYPE_SHORT);
        mqttsMsg.setTopicId(topicId);
        mqttsMsg.setData((uint8_t*)data, (uint8_t)dataLength);
        if (_qos){
            mqttsMsg.setMsgId(getNextMsgId());
        }
        return requestSendMsg((MqttsMessage*)&mqttsMsg);
    }
    return MQTTS_ERR_NO_TOPICID;
}

/*--------- PUBLISH ------*/
int MqttsClient::publish(uint16_t predefinedId, const char* data, int dataLength){
    MqttsPublish mqttsMsg = MqttsPublish();
    mqttsMsg.setFlags(_clientFlg | MQTTS_TOPIC_TYPE_PREDEFINED);
    mqttsMsg.setTopicId(predefinedId);
    mqttsMsg.setData((uint8_t*)data, (uint8_t)dataLength);
    if (_qos){
        mqttsMsg.setMsgId(getNextMsgId());
    }
    return requestSendMsg((MqttsMessage*)&mqttsMsg);

}

/*--------- PUBACK ------*/
int MqttsClient::pubAck(uint16_t topicId, uint16_t msgId, uint8_t rc){
    MqttsPubAck mqttsMsg = MqttsPubAck();
    mqttsMsg.setTopicId(topicId);
    mqttsMsg.setMsgId(msgId);
    mqttsMsg.setReturnCode(rc);
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- REGISTER ------*/
int MqttsClient::registerTopic(MQString* topic){
    MqttsRegister mqttsMsg = MqttsRegister();
    mqttsMsg.setTopicName(topic);
    mqttsMsg.setMsgId(getNextMsgId());
    _topics.addTopic(topic);
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- SUBSCRIBE ------*/
int MqttsClient::subscribe(MQString* topic, TopicCallback callback){
    MqttsSubscribe mqttsMsg = MqttsSubscribe();
    uint16_t topicId = _topics.getTopicId(topic);
    if (topicId){
        mqttsMsg.setTopicId(topicId);
        mqttsMsg.setFlags(_clientFlg | MQTTS_TOPIC_TYPE_NORMAL);
    }else{
        mqttsMsg.setTopicName(topic);
        mqttsMsg.setFlags(_clientFlg | MQTTS_TOPIC_TYPE_NORMAL);
        _topics.addTopic(topic);
        _topics.setCallback(topic, callback);
    }
    mqttsMsg.setMsgId(getNextMsgId());
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- SUBSCRIBE ------*/
int MqttsClient::subscribe(uint16_t predefinedId, TopicCallback callback){
    MqttsSubscribe mqttsMsg = MqttsSubscribe();
    mqttsMsg.setTopicId(predefinedId);
    mqttsMsg.setFlags(_clientFlg | MQTTS_TOPIC_TYPE_PREDEFINED);
    mqttsMsg.setMsgId(getNextMsgId());
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- UNSUBSCRIBE ------*/
int MqttsClient::unsubscribe(MQString* topic){
    MqttsUnsubscribe mqttsMsg = MqttsUnsubscribe();
    uint16_t topicId = _topics.getTopicId(topic);
    if (topicId){
        mqttsMsg.setTopicId(topicId);
    }else{
        mqttsMsg.setTopicName(topic);
    }
    mqttsMsg.setMsgId(getNextMsgId());
    mqttsMsg.setFlags(_clientFlg | MQTTS_TOPIC_TYPE_NORMAL);
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- UNSUBSCRIBE ------*/
int MqttsClient::unsubscribe(uint16_t predefinedId){
    MqttsUnsubscribe mqttsMsg = MqttsUnsubscribe();
    mqttsMsg.setTopicId(predefinedId);
    mqttsMsg.setMsgId(getNextMsgId());
    mqttsMsg.setFlags(_clientFlg | MQTTS_TOPIC_TYPE_PREDEFINED);
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- DISCONNECT ------*/
int MqttsClient::disconnect(uint16_t duration){
    MqttsDisconnect mqttsMsg = MqttsDisconnect();
    if (duration){
        mqttsMsg.setDuration(duration);
    }
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- SEARCHGW ------*/
int  MqttsClient::searchGw(uint8_t radius){
    MqttsSearchGw mqttsMsg = MqttsSearchGw();
    mqttsMsg.setRadius(radius);
    return requestPrioritySendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- PINGREQ ------*/
int  MqttsClient::pingReq(MQString* clientId){
    MqttsPingReq mqttsMsg = MqttsPingReq(clientId);
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- PINGRESP ------*/
int  MqttsClient::pingResp(){
    MqttsPingResp mqttsMsg = MqttsPingResp();
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- WILLTOPIC ------*/
int MqttsClient::willTopic(){
    MqttsWillTopic mqttsMsg = MqttsWillTopic();
    mqttsMsg.setWillTopic(_willTopic);
    mqttsMsg.setFlags(_clientFlg & 0x70);
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/*--------- WILLMSG ------*/
int MqttsClient::willMsg(){
    MqttsWillMsg mqttsMsg = MqttsWillMsg();
    mqttsMsg.setWillMsg(_willMessage);
    return requestSendMsg((MqttsMessage*)&mqttsMsg);
}

/* ===================================================
          Procedures for  Received Messages
 =====================================================*/
void MqttsClient::recieveMessageHandler(ZBRxResponse* recvMsg, int* returnCode){
    if ( _gwHdl.isSearching() && (recvMsg->getData()[1] != MQTTS_TYPE_GWINFO)){
        *returnCode = MQTTS_ERR_NO_ERROR;

/*---------  PUBLISH  --------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_PUBLISH){

        #ifdef DEBUG_MQTTS
            #ifdef ARDUINO
              debug.println("PUBLISH Received");
            #endif
            #ifdef MBED
                debug.fprintf(stdout,"PUBLISH received\n");
            #endif
            #ifdef LINUX
                fprintf(stdout,"PUBLISH received\n");
            #endif
        #endif  /* DEBUG_MQTTS */

        MqttsPublish mqMsg = MqttsPublish();
        mqMsg.setFrame(recvMsg);
        if ( _gwHdl.getAddress16() == getRxRemoteAddress16()){
            *returnCode = _pubHdl.exec(&mqMsg,&_topics);
            if (mqMsg.getQos() && MQTTS_FLAG_QOS_1){
                pubAck(mqMsg.getTopicId(), mqMsg.getMsgId(), MQTTS_RC_ACCEPTED);
            }
        }


/*---------  PUBACK  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_PUBACK && getMsgRequestStatus() == MQTTS_MSG_WAIT_ACK){
        MqttsPubAck mqMsg = MqttsPubAck();
        copyMsg(&mqMsg, recvMsg);

        if (mqMsg.getMsgId() == getLong(_sendQ->getMessage(0)->getBody() + 3)){
            if (mqMsg.getReturnCode() == MQTTS_RC_ACCEPTED){
                setMsgRequestStatus(MQTTS_MSG_COMPLETE);

            }else if (mqMsg.getReturnCode() == MQTTS_RC_REJECTED_CONGESTION){
                  setMsgRequestStatus(MQTTS_MSG_RESEND_REQ);

            }else{
                *returnCode = MQTTS_ERR_REJECTED;
                setMsgRequestStatus(MQTTS_MSG_REJECTED);
            }
        }
          #ifdef DEBUG_MQTTS
              #ifdef ARDUINO
                debug.print("\nPUBACK ReturnCode=");
                debug.println(mqMsg.getReturnCode(),HEX);
                debug.println();
              #endif
              #ifdef MBED
                  debug.fprintf(stdout,"\nPUBACK ReturnCode=%d\n", mqMsg.getReturnCode());
              #endif
              #ifdef LINUX
                  fprintf(stdout,"\nPUBACK ReturnCode=%d\n", mqMsg.getReturnCode());
              #endif
          #endif /* DEBUG_MQTTS */


/*---------  PINGRESP  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_PINGRESP){

        _gwHdl.recvPingResp();
        if (getMsgRequestType() == MQTTS_TYPE_PINGREQ){
            setMsgRequestStatus(MQTTS_MSG_COMPLETE);
        }
        #ifdef DEBUG_MQTTS
            #ifdef ARDUINO
                debug.println(" PINGRESP received");
            #endif
            #ifdef MBED
                debug.fprintf(stdout," PINGRESP received\n");
            #endif
            #ifdef LINUX
                fprintf(stdout," PINGRESP received\n");
            #endif
        #endif /* DEBUG_MQTTS */

/*---------  PINGREQ  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_PINGREQ){

        pingResp();
          #ifdef DEBUG_MQTTS
             #ifdef ARDUINO
                 debug.println(" PINGREQ received");
             #endif
                 #ifdef MBED
                 debug.fprintf(stdout," PINGREQ received\n");
                 #endif
                 #ifdef LINUX
                 fprintf(stdout," PINGREQ received\n");
             #endif
         #endif /* DEBUG_MQTTS */
/*---------  ADVERTISE  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_ADVERTISE){

        *returnCode = MQTTS_ERR_NO_ERROR;

        MqttsAdvertise mqMsg = MqttsAdvertise();
        copyMsg(&mqMsg, recvMsg);
        _gwHdl.recvAdvertise(&mqMsg);
        #ifdef DEBUG_MQTTS
            #ifdef ARDUINO
                debug.println(" ADVERTISE received");
            #endif
                #ifdef MBED
                debug.fprintf(stdout," ADVERTISE received\n");
                #endif
                #ifdef LINUX
                fprintf(stdout," ADVERTISE received\n");
            #endif
        #endif /* DEBUG_MQTTS */


/*---------  GWINFO  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_GWINFO){

        MqttsGwInfo mqMsg = MqttsGwInfo();
        copyMsg(&mqMsg, recvMsg);
        _gwHdl.recvGwInfo(&mqMsg);
        if (getMsgRequestType() == MQTTS_TYPE_SEARCHGW){
            setMsgRequestStatus(MQTTS_MSG_COMPLETE);
        }
        #ifdef DEBUG_MQTTS
           #ifdef ARDUINO
               debug.println(" GWINFO received");
           #endif
               #ifdef MBED
               debug.fprintf(stdout," GWINFO received\n");
               #endif
               #ifdef LINUX
               fprintf(stdout," GWINFO received\n");
           #endif
       #endif /* DEBUG_MQTTS */

/*---------  CANNACK  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_CONNACK){

        if (_qos == 1 && getMsgRequestStatus() == MQTTS_MSG_WAIT_ACK){
            MqttsConnack mqMsg = MqttsConnack();
            copyMsg(&mqMsg, recvMsg);

            if (mqMsg.getReturnCode() == MQTTS_RC_ACCEPTED){
                setMsgRequestStatus(MQTTS_MSG_COMPLETE);
                _gwHdl.setStatus(MQTTS_GW_CONNECTED);
            }else if (mqMsg.getReturnCode() == MQTTS_RC_REJECTED_CONGESTION){
               setMsgRequestStatus(MQTTS_MSG_RESEND_REQ);
            }else{
               setMsgRequestStatus(MQTTS_MSG_REJECTED);
               *returnCode = MQTTS_ERR_REJECTED;
            }
        }
        #ifdef DEBUG_MQTTS
            #ifdef ARDUINO
                debug.println(" CONNACK received");
            #endif
                #ifdef MBED
                debug.fprintf(stdout," CONNACK received\n");
                #endif
                #ifdef LINUX
                fprintf(stdout," CONNACK received\n");
            #endif
        #endif /* DEBUG_MQTTS */
/*---------  REGISTER  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_REGISTER){

        MqttsRegister mqMsg = MqttsRegister();

        mqMsg.setFrame(recvMsg);
        uint16_t topicId = _topics.getTopicId(mqMsg.getTopicName());
        if (topicId == 0){
            if (_topics.match(mqMsg.getTopicName())){
                MQString* mqStr = mqMsg.getTopicName()->create();
                _topics.addTopic(mqStr);
                _topics.setTopicId(mqStr,mqMsg.getTopicId());
                _topics.setCallback(mqMsg.getTopicId(),_topics.match(mqStr)->getCallback());
            }
        }
        #ifdef DEBUG_MQTTS
            #ifdef ARDUINO
                debug.println(" REGISTER received");
            #endif
            #ifdef MBED
                  debug.fprintf(stdout," REGISTER received\n");
            #endif
            #ifdef LINUX
                  fprintf(stdout," REGISTER received\n");
            #endif
        #endif /* DEBUG_MQTTS */

/*---------  REGACK  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_REGACK){
        if (getMsgRequestStatus() == MQTTS_MSG_WAIT_ACK &&
            getMsgRequestType() == MQTTS_TYPE_REGISTER){
            MqttsRegAck mqMsg = MqttsRegAck();
            copyMsg(&mqMsg, recvMsg);
            if (mqMsg.getMsgId() == getLong(_sendQ->getMessage(0)->getBody() + 2)){
                if (mqMsg.getMsgId() == getLong(_sendQ->getMessage(0)->getBody() + 2)){
                    if (getLong((uint8_t*)_sendQ->getMessage(0)->getBody()+4)){
                        if (mqMsg.getReturnCode() == MQTTS_RC_ACCEPTED){
                            setMsgRequestStatus(MQTTS_MSG_COMPLETE);
                            MQString topic;
                            topic.readBuf(_sendQ->getMessage(0)->getBody() + 4);
                            _topics.setTopicId(&topic, mqMsg.getTopicId());
                        }else if (mqMsg.getReturnCode() == MQTTS_RC_REJECTED_CONGESTION){
                          setMsgRequestStatus(MQTTS_MSG_RESEND_REQ);
                        }else{
                            *returnCode = MQTTS_ERR_REJECTED;
                        }
                    }
                }
            }
        }
        #ifdef DEBUG_MQTTS
           #ifdef ARDUINO
               debug.println(" REGACK received");
           #endif
           #ifdef MBED
                 debug.fprintf(stdout," REGACK received\n");
           #endif
           #ifdef LINUX
                 fprintf(stdout," REGACK received\n");
           #endif
       #endif /* DEBUG_MQTTS */

/*---------  SUBACK  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_SUBACK && getMsgRequestStatus() == MQTTS_MSG_WAIT_ACK){
        MqttsSubAck mqMsg = MqttsSubAck();
        copyMsg(&mqMsg, recvMsg);



        if (mqMsg.getMsgId() == getLong(_sendQ->getMessage(0)->getBody() + 1)){
            if (mqMsg.getReturnCode() == MQTTS_RC_ACCEPTED){
                setMsgRequestStatus(MQTTS_MSG_COMPLETE);
                if (_sendQ->getMessage(0)->getBodyLength() > 5){ // TopicName is not Id
                    MQString topic;
                    topic.readBuf(_sendQ->getMessage(0)->getBody() + 3);
                    _topics.setTopicId(&topic, mqMsg.getTopicId());

                }
            }else if (mqMsg.getReturnCode() == MQTTS_RC_REJECTED_CONGESTION){
                setMsgRequestStatus(MQTTS_MSG_REQUEST);
            }else{
                *returnCode = MQTTS_ERR_REJECTED;
            }
        }
        #ifdef DEBUG_MQTTS
            #ifdef ARDUINO
                debug.println();
                debug.print("SUBACK ReturnCode=");
                debug.println(mqMsg.getReturnCode(),HEX);
            #endif
            #if MBED
                debug.fprintf(stdout,"\nSUBACK ReturnCode=%d\n", mqMsg.getReturnCode());
            #endif
            #ifdef LINUX
                fprintf(stdout,"\nSUBACK ReturnCode=%d\n", mqMsg.getReturnCode());
            #endif
        #endif /* DEBUG_MQTTS */

/*---------  UNSUBACK  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_UNSUBACK && getMsgRequestStatus() == MQTTS_MSG_WAIT_ACK){

        MqttsUnSubAck mqMsg = MqttsUnSubAck();
        copyMsg(&mqMsg, recvMsg);
        if (mqMsg.getMsgId() == getLong(_sendQ->getMessage(0)->getBody() + 1)){
              setMsgRequestStatus(MQTTS_MSG_COMPLETE);
        }
        #ifdef DEBUG_MQTTS
           #ifdef ARDUINO
               debug.println(" UNSUBACK received");
           #endif
           #ifdef MBED
                 debug.fprintf(stdout," UNSUBACK received\n");
           #endif
           #ifdef LINUX
                 fprintf(stdout," UNSUBACK received\n");
           #endif
       #endif /* DEBUG_MQTTS */

/*---------  DISCONNECT  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_DISCONNECT && getMsgRequestStatus() == MQTTS_MSG_WAIT_ACK){
         setMsgRequestStatus(MQTTS_MSG_COMPLETE);
         _gwHdl.setStatus(MQTTS_GW_DISCONNECTED);
              #ifdef DEBUG_MQTTS
                   #ifdef ARDUINO
                       debug.println(" DISCONNECT received");
                   #endif
                   #ifdef MBED
                         debug.fprintf(stdout," DISCONNECT received\n");
                   #endif
                   #ifdef LINUX
                         fprintf(stdout," UNSUBACK received\n");
                   #endif
               #endif /* DEBUG_MQTTS */

/*---------  WILLMSGRESP  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_WILLMSGRESP && getMsgRequestStatus() == MQTTS_MSG_WAIT_ACK){
            #ifdef DEBUG_MQTTS
                 #ifdef ARDUINO
                     debug.println(" WILLMSGRESP received");
                 #endif
                 #ifdef MBED
                       debug.fprintf(stdout," WILLMSGRESP received\n");
                 #endif
                 #ifdef LINUX
                       fprintf(stdout," WILLMSGRESP received\n");
                 #endif
             #endif /* DEBUG_MQTTS */

/*---------  WILLTOPICREQ  ----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_WILLTOPICREQ){

        if (getMsgRequestType() == MQTTS_TYPE_CONNECT){
            setMsgRequestStatus(MQTTS_MSG_COMPLETE);
            MqttsWillTopic mqMsg = MqttsWillTopic();
            mqMsg.setWillTopic(_willTopic);

            if (  _sendQ->addPriorityRequest((MqttsMessage*)&mqMsg) == 0){
                setMsgRequestStatus(MQTTS_MSG_REQUEST);
            }else{
                *returnCode = MQTTS_ERR_OUT_OF_MEMORY;
            }
        }
        #ifdef DEBUG_MQTTS
            #ifdef ARDUINO
                debug.println(" WILLTOPICREQ received");
            #endif
            #ifdef MBED
                  debug.fprintf(stdout," WILLTOPICREQ received\n");
            #endif
            #ifdef LINUX
                  fprintf(stdout," WILLTOPICREQ received\n");
            #endif
        #endif /* DEBUG_MQTTS */

/*---------  WILLMSGREQ  -----------*/
    }else if (recvMsg->getData()[1] == MQTTS_TYPE_WILLMSGREQ){

        if (getMsgRequestType() == MQTTS_TYPE_WILLTOPIC){
            setMsgRequestStatus(MQTTS_MSG_COMPLETE);
            MqttsWillMsg mqMsg = MqttsWillMsg();
            mqMsg.setWillMsg(_willMessage);
            if (  _sendQ->addPriorityRequest((MqttsMessage*)&mqMsg) == 0){
                setMsgRequestStatus(MQTTS_MSG_REQUEST);
            }else{
                *returnCode = MQTTS_ERR_OUT_OF_MEMORY;
            }
        }
        #ifdef DEBUG_MQTTS
            #ifdef ARDUINO
                debug.println(" WILLMSGREQ received");
            #endif
            #ifdef MBED
                  debug.fprintf(stdout," WILLMSGREQ received\n");
            #endif
            #ifdef LINUX
                  fprintf(stdout," WILLMSGREQ received\n");
            #endif
        #endif /* DEBUG_MQTTS */
    }
}


/*===================  End of file ====================*/
