#ifndef CONFERENCE_H
#define CONFERENCE_H
#include "Session.hpp"
#include "SilkFileStreamSource.hpp"
int releaseSession(int sid);
int createListen(Session* session, int ssrc);
int startListen(Session* session, int handle);
int stopListen(int session, int handle);
int releaseListen(int session, int handle,int waitMilSec);
Session* raiseSession1(int sid,int ip,short port,string root_dir,bool egross);
int startHeartBeat(Session* session, int ip, short port,int uin, int expired, int interval, int timeout);
int OnLoad(void*reserved);
int getTransactionOutputParam(TransactionOutputParam &param, vl_uint32 ssrc,
                              void* extension, vl_size extensionLen, vl_uint16 extId,
                              vl_uint32 nettype, vl_uint32 netsignal);
int createSpeakNetParam(Session *session, int ssrc, int srcId,
                        int dstId, int sesstype, int netType, int netSignal);

int stopSpeak(Session* session, int handle);
void playLocalFile(string filePath, int ssrc);
int stopPlayLocalFile(int ssrc);
void onMiniteTick();
#endif // CONFERENCE_H
