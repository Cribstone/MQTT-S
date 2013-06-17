/*
 * MQTTS.cpp
 *                            The MIT License (MIT)
 *
 *               Copyright (c) 2013 Tomoaki YAMAGUCHI  All rights reserved.
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
 *  Created on: 2013/06/08
 *      Author: Tomoaki YAMAGUCHI
 *     Version: 0.3.0
 *
 */

#ifndef ARDUINO
        #include "MQTTS_Defines.h"
#else
        #include <MQTTS_Defines.h>
#endif


#ifdef ARDUINO
  #include <SoftwareSerial.h>
  #include <MQTTS.h>

  #if defined(DEBUG_ZBEESTACK) || defined(DEBUG_EMULATION)
	extern SoftwareSerial debug;
  #endif

#endif  /* ARDUINO */

#ifdef MBED
  #include "mbed.h"
  #include "MQTTS.h"

  #if defined(DEBUG_ZBEESTACK) || defined(DEBUG_EMULATION)
	extern Serial debug;
  #endif
#endif  /* MBED */

#ifdef LINUX
  #include "MQTTS.h"
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

/*=====================================
        Global functions
 ======================================*/

uint16_t getLong(uint8_t* pos){
  uint16_t val = ((uint16_t)*pos++ << 8);
  return val += *pos;
}

void setLong(uint8_t* pos, uint16_t val){
    *pos++ = (val >> 8) & 0xff;
    *pos = val &0xff;
}

/*=====================================
        Class MQString
  =====================================*/
MQString::MQString(){
    _length = 0;
    _constStr = NULL;
    _str = NULL;
}

MQString::MQString(const char* str){
    _length = strlen(str);
    if (_length){
        _constStr = str;
    }else{
        _constStr = NULL;
    }
    _str = NULL;
}

MQString::~MQString(){
    if (_str){
        free(_str);
    }
}

uint8_t MQString::getCharLength(){
    return _length;
}
uint8_t MQString::getDataLength(){
    return _length + 2;
}

int MQString::comp(MQString* str){
    if (_str){
        return (strncmp(_str, str->getStr(), _length));
    }else if(_constStr){
        return (strncmp(_constStr, str->getConstStr(), _length));
    }
    return 1;
}

int MQString::comp(const char* str){
    if(_str){
        return (strncmp(_str, str, _length));
    }else if (_constStr){
        return (strncmp(_constStr, str, _length));
    }
    return 1;
}

int MQString::ncomp(MQString* str, long len){
    if (_str){
        return memcmp(_str, str->getStr(), len);
    }else if (_constStr){
        return memcmp(_constStr, str->getConstStr(), len);
    }
    return 1;
}


void MQString::copy(MQString* str){
    _length = str->getCharLength();
    if (str->isConst()){
        _constStr = getConstStr();
    }else{
        _str = str->getStr();
    }
}

MQString* MQString::create(){
    char* newStr = (char*)calloc(getDataLength(), sizeof(char));
    memcpy(newStr, getConstStr(), getCharLength());
    MQString* newPtr = new MQString((const char*)newStr);
    return newPtr;
}

void MQString::copy(const char* str){
    _length = strlen(str);
    _constStr = str;
    _str = NULL;
    freeStr();
}

void MQString::copy(char* str){
    _length = strlen(str);
    freeStr();
    _str = (char*)calloc(_length, sizeof(char));
    _constStr = NULL;
    memcpy(_str, str, _length);
}

void MQString::writeBuf(uint8_t* buf){
    if (_str){
        memcpy(buf + 2, _str, _length);
    }else if (_constStr){
        memcpy(buf + 2, _constStr, _length);
    }
    setLong(buf, _length);
}

void MQString::readBuf(uint8_t* buf){
    _str = NULL;
    _constStr = (const char*)buf + 2;
    _length = getLong(buf);
}

uint8_t MQString::getChar(long index){
    if (_str){
        return (index < _length ? _str[index]: 0);
    }else if (_constStr){
        return (index < _length ? _constStr[index]: 0);
    }
    return 0;
}

char* MQString::getStr(){
    return (_str ? _str : NULL);
}

const char* MQString::getConstStr(){
    return (_constStr ? _constStr : NULL);
}


bool MQString::isConst(){
  return (_constStr ? true : false);
}

void MQString::freeStr(){
    if (_str){
        free(_str);
        _str = NULL;
    }
}


/*=====================================
        Class MqttsMessage
 ======================================*/
MqttsMessage::MqttsMessage(){
    _msgBuff = NULL;
    _length = 0;
    _status = 0;
    _type = 0;
}
MqttsMessage::~MqttsMessage(){
    if (_msgBuff != NULL){
        delete(_msgBuff);
    }
}

void MqttsMessage::reset(){
    _msgBuff = NULL;
    _length = 0;
    _status = 0;
    _type = 0;
}

void MqttsMessage::setType(uint8_t type){
    _type = type;
    if ( _msgBuff != NULL){
        _msgBuff[1] = type;
    }
}

void MqttsMessage::setLength(uint8_t length){
    _length = length;
    if ( _msgBuff != NULL){
        _msgBuff[0] = length;
    }
}

bool MqttsMessage::setBody(uint8_t* body){
    if (allocateBody()) {
        _msgBuff[0] = _length;
        _msgBuff[1] = _type;
        memcpy(_msgBuff + 2, body, _length - MQTTS_HEADER_SIZE);
        return true;
    }else{
        return false;
    }
}

bool MqttsMessage::allocateBody(){
    if ( _length ) {
        if (_msgBuff){
              free(_msgBuff);
        }
        _msgBuff = (uint8_t*)calloc(_length, sizeof(uint8_t));
        if ( _msgBuff){
            _msgBuff[0] = _length;
            _msgBuff[1] = _type;
            return true;
        }else{
            return false;
        }
    }else{
        return false;
    }
}

void MqttsMessage::setStatus(uint8_t stat){
    _status = stat;
}

uint8_t MqttsMessage::getLength(){
    return _length;
}

uint8_t MqttsMessage::getType(){
    return _type;
}

uint8_t MqttsMessage::getStatus(){
    return _status;
}

uint8_t* MqttsMessage::getBody(){
    return _msgBuff + MQTTS_HEADER_SIZE;
}

uint8_t MqttsMessage::getBodyLength(){
    return _length - MQTTS_HEADER_SIZE;
}

uint8_t MqttsMessage::getFrameLength(){
    return _length;
}

uint8_t* MqttsMessage::getMsgBuff(){
    return _msgBuff;
}

void MqttsMessage::setMsgBuff(uint8_t* msgBuff){
    _msgBuff = msgBuff;
}

bool MqttsMessage::copy(MqttsMessage* src){
    setLength(src->getLength());
    setType(src->getType());
    setStatus(src->getStatus());
    _msgBuff = src->_msgBuff;
    src->setMsgBuff(NULL);
    if (_msgBuff == NULL){
        return false;
    }
    return true;
}

/*=====================================
        Class MqttsAdvrtise
 ======================================*/
MqttsAdvertise::MqttsAdvertise():MqttsMessage(){
    setLength(5);
    setType(MQTTS_TYPE_ADVERTISE);
    allocateBody();
}

MqttsAdvertise::~MqttsAdvertise(){

}

void MqttsAdvertise::setGwId(uint8_t id){
    getBody()[0] = id;
}

void MqttsAdvertise::MqttsAdvertise::setDuration(uint16_t duration){
    uint8_t* pos = getBody() + 1;
    setLong(pos, duration);
}

uint8_t MqttsAdvertise::getGwId(){
    return getBody()[0];
}

uint16_t MqttsAdvertise::getDuration(){
  uint8_t* pos = getBody() + 1;
    return getLong(pos);
}

/*=====================================
        Class MqttsSearchgw
 ======================================*/
MqttsSearchGw::MqttsSearchGw():MqttsMessage(){
    setLength(3);
    setType(MQTTS_TYPE_SEARCHGW);
    allocateBody();
}

MqttsSearchGw::~MqttsSearchGw(){

}

void MqttsSearchGw::setRadius(uint8_t radius){
  getBody()[0] = radius;
}

uint8_t MqttsSearchGw::getRadius(){
  return getBody()[0];
}

/*=====================================
        Class MqttsGwinfo
 ======================================*/
MqttsGwInfo::MqttsGwInfo(){
  setLength(3);
  setType(MQTTS_TYPE_GWINFO);
  allocateBody();
}

MqttsGwInfo::~MqttsGwInfo(){

}

uint8_t MqttsGwInfo::getGwId(){
    return getBody()[0];
}

void MqttsGwInfo::setGwId(uint8_t id){
    getBody()[0] = id;
}

/*=====================================
         Class MqttsConnect
  ======================================*/
MqttsConnect::MqttsConnect(MQString* id){
    setLength(id->getDataLength() + 6);
    allocateBody();
    setType(MQTTS_TYPE_CONNECT);
    getBody()[1] = MQTTS_PROTOCOL_ID;
    id->writeBuf(getBody() + 4);
}

MqttsConnect::~MqttsConnect(){

}

void MqttsConnect::setFlags(uint8_t flg){
    getBody()[0] = flg & 0x0c;
}

uint8_t MqttsConnect::getFlags(){
    return getBody()[0];
}

void MqttsConnect::setDuration(uint16_t msec){
    setLong((uint8_t*)getBody() + 2, msec);
}

uint16_t MqttsConnect::getDuration(){
    return getLong((uint8_t*)getBody() + 2);
}

void MqttsConnect::setClientId(MQString* id){
    id->writeBuf(getBody() + 4);
    setLength(id->getDataLength() + 6);
}

uint8_t* MqttsConnect::getClientId(){
    return getBody() + 6;
}

void MqttsConnect::setFrame(uint8_t* data, uint8_t len){
    setLength(len + MQTTS_HEADER_SIZE);
    allocateBody();
    memcpy(getBody(), data, len);
    getBody()[2] = getLong(data + 4);
    getBody()[0] = *(data + 2);
}

/*=====================================
        Class MqttsConnack
 ======================================*/
MqttsConnack::MqttsConnack(){
    setLength(3);
    setType(MQTTS_TYPE_CONNACK);
    allocateBody();
}

MqttsConnack::~MqttsConnack(){

}

void MqttsConnack::setReturnCode(uint8_t rc){
    getBody()[0] = rc;
}

uint8_t MqttsConnack::getReturnCode(){
    return getBody()[0];
}

/*=====================================
       Class MqttsWillTopicReq
======================================*/
MqttsWillTopicReq::MqttsWillTopicReq(){
    setLength(2);
    setType(MQTTS_TYPE_WILLTOPICREQ);
    allocateBody();
}

MqttsWillTopicReq::~MqttsWillTopicReq(){

}

/*=====================================
         Class MqttsWillTopic
  ======================================*/
MqttsWillTopic::MqttsWillTopic(){
    setLength(3);
    setType(MQTTS_TYPE_WILLTOPIC);
    allocateBody();
    _flags = 0;
}

MqttsWillTopic::~MqttsWillTopic(){

}

void MqttsWillTopic::setFlags(uint8_t flags){
    flags &= 0x70;
    if (_msgBuff){
            getBody()[0] = flags;
    }
    _flags = flags;
}

void MqttsWillTopic::setWillTopic(MQString* topic){
    setLength(topic->getDataLength() + 3);
    allocateBody();
    topic->writeBuf(getBody() + 1);
    _msgBuff[2] = _flags;
    _ustring.copy(topic);
}

MQString* MqttsWillTopic::getWillTopic(){
  if (_msgBuff){
        return &_ustring;
    }else{
        return NULL;
    }
}

bool MqttsWillTopic::isWillRequired(){
    return getBody()[0] & 0b00010000;
}

uint8_t MqttsWillTopic::getQos(){
    return _flags && 0b01100000;
}

/*=====================================
         Class MqttsWillMsgReq
  ======================================*/
MqttsWillMsgReq::MqttsWillMsgReq(){
    setLength(2);
    setType(MQTTS_TYPE_WILLMSGREQ);
    allocateBody();

}

MqttsWillMsgReq::~MqttsWillMsgReq(){

}

/*=====================================
         Class MqttsWillMsg
  ======================================*/
MqttsWillMsg::MqttsWillMsg(){
    setLength(2);
    setType(MQTTS_TYPE_WILLMSG);
    allocateBody();
}

MqttsWillMsg::~MqttsWillMsg(){

}

void MqttsWillMsg::setWillMsg(MQString* msg){
    setLength(2 + msg->getDataLength());
    allocateBody();
    msg->writeBuf(getBody());
}

char* MqttsWillMsg::getWillMsg(){
    if (_msgBuff){
            return (char*)getBody();
    }else{
            return NULL;
    }
}

/*=====================================
         Class MqttsRegister
  ======================================*/
MqttsRegister::MqttsRegister(){
    setLength(6);
    setType(MQTTS_TYPE_REGISTER);
    allocateBody();
    _topicId = 0;
    _msgId = 0;
}

MqttsRegister::~MqttsRegister(){

}

void MqttsRegister::setTopicId(uint16_t topicId){
  if (_msgBuff){
            setLong(getBody(), topicId);
    }
    _topicId = topicId;
}
uint16_t MqttsRegister::getTopicId(){
    return _topicId;
}
void MqttsRegister::setMsgId(uint16_t msgId){
    if (_msgBuff){
            setLong(getBody() + 2, msgId);
    }
    _msgId = msgId;
}
uint16_t MqttsRegister::getMsgId(){
    return _msgId;

}
void MqttsRegister::setTopicName(MQString* topicName){
    setLength(6 + topicName->getDataLength());
    allocateBody();
    topicName->writeBuf(getBody() + 4);
    setTopicId(_topicId);
    setMsgId(_msgId);
}

void MqttsRegister::setFrame(uint8_t* data, uint8_t len){
    setLength(len + MQTTS_HEADER_SIZE);
    allocateBody();
    memcpy(getBody(), data, len);
    _topicId = getLong(data);
    _msgId = getLong(data + 2);
    _ustring.readBuf(getBody() + 4);
}

void MqttsRegister::setFrame(ZBRxResponse* resp){
    setFrame(resp->getData() + MQTTS_HEADER_SIZE, resp->getData()[0] - MQTTS_HEADER_SIZE);
}

MQString* MqttsRegister::getTopicName(){
    return &_ustring;
}

/*=====================================
         Class MqttsRegAck
  ======================================*/
MqttsRegAck::MqttsRegAck(){
    setLength(7);
    setType(MQTTS_TYPE_REGACK);
    allocateBody();
}
MqttsRegAck::~MqttsRegAck(){

}
void MqttsRegAck::setTopicId(uint16_t topicId){
    setLong((uint8_t*)getBody(), topicId);
}
uint16_t MqttsRegAck::getTopicId(){
    return getLong((unsigned char*)getBody());
}
void MqttsRegAck::setMsgId(uint16_t msgId){
    setLong(getBody()+ 2,msgId);
}
uint16_t MqttsRegAck::getMsgId(){
    return getLong((unsigned char*)getBody()+ 2);
}
void MqttsRegAck::setReturnCode(uint8_t rc){
    getBody()[4] = rc;
}
uint8_t MqttsRegAck::getReturnCode(){
    return (uint8_t)getBody()[4];
}

/*=====================================
         Class MqttsPublish
  ======================================*/
MqttsPublish::MqttsPublish(){
    setLength(7);
    setType(MQTTS_TYPE_PUBLISH);
    allocateBody();
    _topicId = 0;
    _msgId = 0;
    _flags = 0;
}

MqttsPublish::~MqttsPublish(){

}

void MqttsPublish::setFlags(uint8_t flags){
    _flags = flags & 0xf3;
    getBody()[0] = _flags ;
}

uint8_t MqttsPublish::getFlags(){
    return _flags;
}

uint8_t MqttsPublish::getTopicType(){
    return _flags && 0b00000011;
}

bool MqttsPublish::isRetain(){
    return _flags && 0b000010000;
}

uint8_t MqttsPublish::getQos(){
    return _flags && 0b01100000;
}

void MqttsPublish::setTopicId(uint16_t id){
    setLong((uint8_t*)(getBody() + 1), id);
    _topicId = id;
}

uint16_t MqttsPublish::getTopicId(){
    return _topicId;
}
void MqttsPublish::setMsgId(uint16_t msgId){
    setLong((uint8_t*)(getBody() + 3), msgId);
    _msgId = msgId;
}

uint16_t MqttsPublish::getMsgId(){
    return _msgId;
}


void MqttsPublish::setData(uint8_t* data, uint8_t len){
    setLength(7 + len);
    allocateBody();
    memcpy(getBody() + 5, data, len);
    setTopicId(_topicId);
    setMsgId(_msgId);
    setFlags(_flags);
}

uint8_t*  MqttsPublish::getData(){
    return (uint8_t*)(getBody() + 5);
}

void MqttsPublish::setFrame(uint8_t* data, uint8_t len){
    setLength(len + MQTTS_HEADER_SIZE);
    allocateBody();
    memcpy(getBody(), data, len);
    _topicId = getLong(data + 1);
    _msgId = getLong(data + 3);
    _flags = *data;
}


void MqttsPublish::setFrame(ZBRxResponse* resp){
    setFrame(resp->getData() + MQTTS_HEADER_SIZE, resp->getData()[0] - MQTTS_HEADER_SIZE);
}

/*=====================================
         Class MqttsPubAck
 ======================================*/
MqttsPubAck::MqttsPubAck(){
    setLength(7);
    setType(MQTTS_TYPE_PUBACK);
    allocateBody();
}
MqttsPubAck::~MqttsPubAck(){

}
void MqttsPubAck::setTopicId(uint16_t topicId){
    setLong((uint8_t*)getBody(), topicId);
}
uint16_t MqttsPubAck::getTopicId(){
    return getLong((unsigned char*)getBody());
}
void MqttsPubAck::setMsgId(uint16_t msgId){
    setLong(getBody()+ 2,msgId);
}
uint16_t MqttsPubAck::getMsgId(){
    return getLong((unsigned char*)getBody()+ 2);
}
void MqttsPubAck::setReturnCode(uint8_t rc){
    getBody()[4] = rc;
}
uint8_t MqttsPubAck::getReturnCode(){
    return (uint8_t)getBody()[4];
}

 /*=====================================
         Class MqttsSubscribe
  ======================================*/
MqttsSubscribe::MqttsSubscribe(){
    setLength(5);
    setType(MQTTS_TYPE_SUBSCRIBE);
    allocateBody();
    _topicId = 0;
    _msgId = 0;
    _flags = 0;
}

MqttsSubscribe::~MqttsSubscribe(){

}

void MqttsSubscribe::setFlags(uint8_t flags){
    _flags = flags  & 0xe3;
    if (_msgBuff){
              getBody()[0] = _flags;
      }
}

uint8_t MqttsSubscribe::getFlags(){
    return _flags;
}

uint8_t MqttsSubscribe::getQos(){
    return _flags && 0b01100000;
}

void MqttsSubscribe::setTopicId(uint16_t predefinedId){
    setLength(7);
    allocateBody();
    setMsgId(_msgId);
    setLong((uint8_t*)(getBody() + 3), predefinedId);
    setFlags(_flags | MQTTS_TOPIC_TYPE_PREDEFINED);
    _topicId = predefinedId;
}

uint16_t MqttsSubscribe::getTopicId(){
    if (_msgBuff){
        _topicId = getLong(getBody() +3);
    }
    return _topicId;
}
void MqttsSubscribe::setMsgId(uint16_t msgId){
    _msgId = msgId;
    if (_msgBuff){
       setLong((uint8_t*)(getBody() + 1), msgId);
    }
}

uint16_t MqttsSubscribe::getMsgId(){
    if (_msgBuff){
        _msgId = getLong(getBody() + 1);
    }
    return _msgId;
}
void MqttsSubscribe::setTopicName(MQString* data){
    setLength(6 + data->getDataLength());
    allocateBody();
    data->writeBuf(getBody() + 3);
    setMsgId(_msgId);
    setFlags(_flags | MQTTS_TOPIC_TYPE_NORMAL);
    _ustring.copy(data);
}


MQString*  MqttsSubscribe::getTopicName(){
    return &_ustring;
}

void MqttsSubscribe::setFrame(uint8_t* data, uint8_t len){
    setLength(len + MQTTS_HEADER_SIZE);
    allocateBody();
    memcpy(getBody(), data, len);
    _msgId = getLong(data + 1);
    _flags = *data;
    if ((_flags & MQTTS_TOPIC_TYPE) == MQTTS_TOPIC_TYPE_NORMAL){
        _topicId = 0;
        _ustring.readBuf(data + 3);
    }else{
        _topicId = getLong(data + 3);
    }

}

void MqttsSubscribe::setFrame(ZBRxResponse* resp){
    setFrame(resp->getData() + MQTTS_HEADER_SIZE, resp->getData()[0] - MQTTS_HEADER_SIZE);
}

/*=====================================
         Class MqttsSubAck
  ======================================*/
MqttsSubAck::MqttsSubAck(){
    setLength(8);
    setType(MQTTS_TYPE_SUBACK);
    allocateBody();
}

MqttsSubAck::~MqttsSubAck(){

}

void MqttsSubAck::setFlags(uint8_t flags){
    getBody()[0] = flags & 0x60;
}

uint8_t MqttsSubAck::getFlags(){
    return getBody()[0];
}

uint8_t MqttsSubAck::getQos(){
    return getBody()[0] && 0b01100000;
}

void MqttsSubAck::setTopicId(uint16_t id){
    setLong((uint8_t*)(getBody() + 1), id);
}

uint16_t MqttsSubAck::getTopicId(){
    return getLong(getBody() + 1);
}
void MqttsSubAck::setMsgId(uint16_t msgId){
   setLong((uint8_t*)(getBody() + 3), msgId);
}

uint16_t MqttsSubAck::getMsgId(){
    return getLong(getBody() + 3);
}
void MqttsSubAck::setReturnCode(uint8_t rc){
    getBody()[6] = rc;
}
uint8_t  MqttsSubAck::getReturnCode(){
    return getBody()[5];
}


 /*=====================================
         Class MqttsUnsubscribe
  ======================================*/
MqttsUnsubscribe::MqttsUnsubscribe() : MqttsSubscribe(){
    setType(MQTTS_TYPE_UNSUBSCRIBE);
}
MqttsUnsubscribe::~MqttsUnsubscribe(){

}
void MqttsUnsubscribe::setFlags(uint8_t flags){
  if (_msgBuff){
              getBody()[0] = flags & 0xe3;
    }
}
/*=====================================
         Class MqttsUnSubAck
  ======================================*/
MqttsUnSubAck::MqttsUnSubAck(){
    setLength(4);
    setType(MQTTS_TYPE_UNSUBACK);
    allocateBody();
}

MqttsUnSubAck::~MqttsUnSubAck(){

}

void MqttsUnSubAck::setMsgId(uint16_t msgId){
    setLong((uint8_t*)getBody(), msgId);
}

uint16_t MqttsUnSubAck::getMsgId(){
    return getLong(getBody());
}

/*=====================================
        Class MqttsPingReq
 ======================================*/
MqttsPingReq::MqttsPingReq(MQString* id){
  setLength(id->getDataLength() + 2);
  setType(MQTTS_TYPE_PINGREQ);
  allocateBody();
  id->writeBuf(getBody());

}
MqttsPingReq::~MqttsPingReq(){

}

char* MqttsPingReq::getClientId(){
    return (char*)getBody();
}

/*=====================================
        Class MqttsPingResp
 ======================================*/
MqttsPingResp::MqttsPingResp(){
    setLength(2);
    setType(MQTTS_TYPE_PINGRESP);
    allocateBody();
}
MqttsPingResp::~MqttsPingResp(){

}

 /*=====================================
         Class MqttsDisconnect
  ======================================*/
MqttsDisconnect::MqttsDisconnect(){
    setLength(4);
    setType(MQTTS_TYPE_DISCONNECT);
    allocateBody();

}
MqttsDisconnect::~MqttsDisconnect(){

}
void MqttsDisconnect::setDuration(uint16_t duration){
    setLong((uint8_t*)getBody(), duration);
}
uint16_t MqttsDisconnect::getDuration(){
    return getLong((uint8_t*)getBody());
}


/*=====================================
        Class Topic
 ======================================*/
Topic::Topic(){
    _topicStr = NULL;
    _callback = NULL;
    _topicId = 0;
    _status = 0;
}

Topic::~Topic(){
    if (_topicStr){
        delete _topicStr;
    }
}

uint8_t Topic::getStatus(){
    return _status;
}

uint16_t Topic::getTopicId(){
    return _topicId;
}

MQString* Topic::getTopicName(){
    return _topicStr;
}

uint8_t Topic::getTopicLength(){
    return _topicStr->getCharLength();
}

TopicCallback Topic::getCallback(){
    return _callback;
}

void Topic::setTopicId(uint16_t id){
    _topicId = id;
}

void Topic::setStatus(uint8_t stat){
    _topicId = stat;
}


void Topic::setTopicName(MQString* topic){
    _topicStr = topic;
}

void Topic::setCallback(TopicCallback callback){
    _callback = callback;
}

int Topic::execCallback(MqttsPublish* msg){
    if(_callback != NULL){
        return _callback(msg);
    }
    return 0;
}

void Topic::copy(Topic* src){
    setTopicId(src->getTopicId());
    setStatus(src->getStatus());
    setCallback(src->getCallback());
    setCallback(_callback);
    _topicStr = src->getTopicName();
}

uint8_t Topic::isWildCard(){
    if (getTopicName()->getChar(getTopicName()->getCharLength() - 1) == MQTTS_TOPIC_SINGLE_WILDCARD){
        return MQTTS_TOPIC_SINGLE_WILDCARD;
    }else if (getTopicName()->getChar(getTopicName()->getCharLength() - 1) == MQTTS_TOPIC_MULTI_WILDCARD){
        return MQTTS_TOPIC_MULTI_WILDCARD;
    }
    return 0;
}

bool Topic::isMatch(Topic* wildCard){
    uint8_t pos = wildCard->getTopicName()->getCharLength() - 1;
    if (wildCard->isWildCard() == MQTTS_TOPIC_SINGLE_WILDCARD &&
        getTopicName()->ncomp(wildCard->getTopicName(), (long)pos) == 0 ){
        for(; pos < getTopicName()->getCharLength(); pos++){
            if (getTopicName()->getChar(pos) == '/'){
                return false;
            }
        }
        return true;
    }else if(wildCard->isWildCard() == MQTTS_TOPIC_MULTI_WILDCARD &&
        getTopicName()->ncomp(wildCard->getTopicName(), (long)pos) == 0 ){
        return true;
    }
    return false;
}

/*=====================================
        Class Topics
 ======================================*/
Topics::Topics(){
	_sizeMax = 0;
	_elmCnt = 0;
	_topics = NULL;
}

Topics::~Topics() {

}

bool Topics::allocate(uint8_t size){
    _elmCnt = 0;
      _topics = (Topic*)calloc(size, sizeof(Topic));
    if (_topics == 0){
        _sizeMax = 0;
        return false;
    }else{
        _sizeMax = size;
        return true;
    }
}


uint16_t Topics::getTopicId(MQString* topic){
    Topic *p = getTopic(topic);
    if ( p != NULL) {
        return p->getTopicId();
    }
    return 0;
}


Topic* Topics::getTopic(MQString* topic) {
    for (int i = 0; i < _elmCnt; i++) {
        if ( topic->comp(_topics[i].getTopicName()) == 0) {
            return &_topics[i];
        }
    }
    return NULL;
}

Topic* Topics::getTopic(uint16_t id) {
    for (int i = 0; i < _elmCnt; i++) {
        if ( _topics[i].getTopicId() == id) {
            return &_topics[i];
        }
    }
      return NULL;
}

bool Topics::setTopicId(MQString* topic, uint16_t id){
    Topic* p = getTopic(topic);
    if ( p != NULL) {
        p->setTopicId(id);
        return true;
    }else{
        return false;
    }
}

bool Topics::setCallback(MQString* topic, TopicCallback callback){
    Topic* p = getTopic(topic);
    if ( p != NULL) {
        p->setCallback(callback);
        return true;
    }else{
        return false;
    }
}
bool Topics::setCallback(uint16_t topicId, TopicCallback callback){
    Topic* p = getTopic(topicId);
    if ( p != NULL) {
        p->setCallback(callback);
        return true;
    }else{
        return false;
    }
}
int Topics::execCallback(uint16_t topicId, MqttsPublish* msg){
    Topic* p = getTopic(topicId);
    if ( p != NULL) {
        return p->execCallback(msg);
    }
    return 0;
}

void Topics::addTopic(MQString* topic){
    if (getTopic(topic) == NULL){
        /*
      if ( _elmCnt < _sizeMax){
          Topic* saveTopics = _topics;
          Topic* newTopics = (Topic*)calloc(_elmCnt + 1, sizeof(Topic));
          if (newTopics != NULL){
              _topics = newTopics;
              for(int i = 0; i < _elmCnt; i++){
                  _topics[i].copy(&saveTopics[i]);
                  saveTopics[i].setTopicName((MQString*)NULL);
              }

              _topics[_elmCnt].setTopicName(topic);
              _elmCnt++;
              if (saveTopics){
                  delete saveTopics;
              }
          }
      }
      */
        if ( _elmCnt < _sizeMax){
            _topics[_elmCnt].setTopicName(topic);
            _elmCnt++;
        }else{
            Topic* saveTopics = _topics;
            Topic* newTopics = (Topic*)calloc(_sizeMax += MQTTS_MAX_TOPICS, sizeof(Topic));
            if (newTopics != NULL){
                _topics = newTopics;
                for(int i = 0; i < _elmCnt; i++){
                    _topics[i].copy(&saveTopics[i]);
                    saveTopics[i].setTopicName((MQString*)NULL);
                }

                _topics[_elmCnt].setTopicName(topic);
                _elmCnt++;
                if (saveTopics){
                    delete saveTopics;
                }
            }
        }
    }
}

Topic* Topics::match(MQString* topic){
    for ( int i = 0; i< _elmCnt; i++){
        if (_topics[i].isWildCard()){
            if (getTopic(topic)->isMatch(&_topics[i])){
               return &_topics[i];
            }
        }
    }
    return NULL;
}

void Topics::setSize(uint8_t size){
    _sizeMax = size;
}


/*=====================================
        Class SendQue
 ======================================*/
SendQue::SendQue(){
    _queCnt = 0;
    _queSize = 5;
}
SendQue::~SendQue(){
    for( int i = 0; i < _queCnt; i++){
        delete _msg[i];
    }
}

int SendQue::addRequest(MqttsMessage* msg){
    if ( _queCnt < _queSize){
          #ifdef DEBUG_MQTTS
              #ifdef ARDUINO
                debug.print("\nAdd SendQue MsgType = 0x");
                debug.println(msg->getType(), HEX);
              #endif
              #ifdef MBED
                debug.fprintf(stdout,"\nAdd SendQue MsgType = 0x%x\n", msg->getType());
              #endif
              #ifdef LINUX
                fprintf(stdout,"\nAdd SendQue MsgType = 0x%x\n", msg->getType());
              #endif
          #endif /*DEBUG_MQTTS*/
        _msg[_queCnt] =new MqttsMessage();
        _msg[_queCnt++]->copy(msg);
        return _queCnt - 1;
    }
    return MQTTS_ERR_CANNOT_ADD_REQUEST; // Over Que size
}

int SendQue::addPriorityRequest(MqttsMessage* msg){
    if ( _queCnt < _queSize){
          #ifdef DEBUG_MQTTS
              #ifdef ARDUINO
                debug.print("\nAdd SendQue Top MsgType = 0x");
                debug.println(msg->getType(), HEX);
              #endif
              #ifdef MBED
                debug.fprintf(stdout,"\nAdd SendQue Top MsgType = 0x%x\n", msg->getType());
              #endif
              #ifdef LINUX
                fprintf(stdout,"\nAdd SendQue Top MsgType = 0x%x\n", msg->getType());
              #endif
          #endif /*DEBUG_MQTTS*/
        for(int i = _queCnt; i > 0; i--){
            _msg[i] = _msg[i - 1];
        }
        _msg[0] = new MqttsMessage();
        _msg[0]->copy(msg);
        _queCnt++;
        return 0;
    }
    return MQTTS_ERR_CANNOT_ADD_REQUEST;
}

int SendQue::deleteRequest(uint8_t index){
    if ( index < _queCnt){
          #ifdef DEBUG_MQTTS
               #ifdef ARDUINO
                 debug.println("\nDelete SendQue");
               #endif
          #ifdef MBED
                 debug.fprintf(stdout,"\nDelete SendQue\n");
          #endif
          #ifdef LINUX
                 fprintf(stdout,"\nDelete SendQue\n");

                 #endif
          #endif /*DEBUG_MQTTS*/
        delete _msg[index];
        _queCnt--;
        for(int i = index; i < _queCnt; i++){
            _msg[i] = _msg[i + 1];
        }
        for(int i = _queCnt; i < _queSize; i++){
            _msg[i] = NULL;
        }
        return 0;
    }
    return -2;
}

void   SendQue::deleteAllRequest(){
    while ( _queCnt > 0){
        deleteRequest(0);
    }
}

void SendQue::setStatus(uint8_t index, uint8_t status){
    if ( index < _queCnt){
        _msg[index]->setStatus(status);
    }
}

void SendQue::setQueSize(uint8_t sz){
  _queSize = sz;
}

MqttsMessage* SendQue::getMessage(uint8_t index){
  if ( index < _queCnt){
      return _msg[index];
  }
  return NULL;
}

uint8_t SendQue::getStatus(uint8_t index){
  if ( index < _queCnt){
      return _msg[index]->getStatus();
  }
  return -1;
}

uint8_t SendQue::getCount(){
   return _queCnt;
}

/*=====================================
        Class PublishHandller
 ======================================*/
PublishHandller::PublishHandller(){

}
PublishHandller::~PublishHandller(){

}
int PublishHandller::exec(MqttsPublish* msg, Topics* topics){
    if (topics->getTopic(msg->getTopicId())){
        return topics->getTopic(msg->getTopicId())->execCallback(msg);
    }
    return 0;
}



/////////////////// End of File ///////////////
