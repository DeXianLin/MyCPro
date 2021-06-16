#include "conference.h"
#include "StreamPlayScheduler.hpp"
#include "RTPHeartBeat.hpp"
#include <iostream>
#include <WinSock2.h>
#include <map>
#include <ptt.h>
#include <signal.h>
static const char* g_media_repport_ip = "112.124.1.212";
static const unsigned short g_media_report_port = 33446;
static uint32_t g_report_uin;

std::map<int, Session*> activeSession;//会话容器
pthread_mutex_t g_session_mutex = PTHREAD_MUTEX_INITIALIZER;


static uint8_t gReportConvertNettype(uint8_t netType) { //报告转换网类型
    uint8_t report_type = 0;
    switch (netType) {
    case 1: // 2G
        report_type = 2;
        break;
    case 2: // 3G
        report_type = 3;
        break;
    case 3:  // wifi
        report_type = 1;
        break;
    }

    return report_type;
}

// 暂时通过流媒体回调设置传入
int OS_API_VERSION = -1;
static uint32_t convert_x(uint32_t x)
{
    return x>>24 | x>>8&0xff00 | x<<8&0xff0000 | x<<24;
}
//这个函数完成和htobe64一样的功能
static uint64_t convert(uint64_t x)
{
    return convert_x(x)+0ULL<<32 | convert_x(x>>32);
}
class StreamPlayCallback: public StreamPlayObserver
{
private:
    int send;

public:
    void reportToServer(void* data, vl_size len)
    {
        int sock_fd = 0;
        struct sockaddr_in remote;

        memset(&remote,0, sizeof(struct sockaddr_in));
        remote.sin_family = AF_INET;
        remote.sin_addr.s_addr = inet_addr(g_media_repport_ip);
        remote.sin_port = htons(g_media_report_port);

        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        sendto(sock_fd, (char*)data, len, 0, (struct sockaddr *) &remote, sizeof(struct sockaddr_in));
        closesocket(sock_fd);
    }

    void reportRecvSummary(struct RecvTransInfo& recvInfo)
    {
        RecvTransInfo netInfo;
        netInfo.first_recv_ts = convert(recvInfo.first_recv_ts);
        netInfo.play_ts = convert(recvInfo.play_ts);
        netInfo.max_jitter = htonl(recvInfo.max_jitter);
        netInfo.total_jitter = htonl(recvInfo.total_jitter);
        netInfo.max_seq = htonl(recvInfo.max_seq);
        netInfo.ssrc = htonl(recvInfo.ssrc);
        netInfo.send_id = htonl(recvInfo.send_id);
        netInfo.group_id = htonl(recvInfo.group_id);
        netInfo.recv_id = htonl(g_report_uin);  // 固定为本地uin
        netInfo.flag = htons(recvInfo.flag);
        netInfo.dur_per_pack = htons(recvInfo.dur_per_pack);
        netInfo.block_times = htons(recvInfo.block_times);
        netInfo.normalEnd = recvInfo.normalEnd;
        netInfo.lost_persent = recvInfo.lost_persent;
        netInfo.send_os = recvInfo.send_os;
        netInfo.recv_os = 0;
        netInfo.send_net_type = recvInfo.send_net_type;
        /* jni反射获取本地网络类型 */
        netInfo.recv_net_type =1;
        reportToServer(&netInfo, sizeof(struct RecvTransInfo));
    }
    /*
     * 开始接收流媒体数据事件
     */
    void onReceiveStart(vl_uint32 ssrc) {

        return;
    }
    /*
     * 结束接收流媒体数据事件
     */
    void onReceiveEnd(vl_uint32 ssrc) {

        return;
    }

    /**
     * 流媒体处理完毕，并已经释放占用资源
     */
    void onRelease(vl_uint32 ssrc, struct SpsResultInfo& info) {

        /* 上报接收情况 */
        reportRecvSummary(info.streamInfoDetail.recfInfo);

    }

    /**
     * 开始播放流媒体事件
     */
    void onPlayStarted(vl_uint32 ssrc) {
        return;
    }
    /**
     * 结束播放流媒体事件
     */
    void onPlayStoped(vl_uint32 ssrc) {
        return ;
    }

    void onMediaHeartbeatTimeout()
    {
        return;
    }
    void onNewIncomingTransByJNI(int type, int send_id, int recv_id, int ssrc)
    {

        return;
    }

    void onMediaHeartbeatRecv(int i)
    {
        return;
    }
    void onMediaHeartbeatFailed()
    {
        return;
    }

};

StreamPlayCallback mediaCallback;

Session * getSessionById(int sid)
{
    Session *sess = NULL;
    pthread_mutex_lock(&g_session_mutex);
    std::map<int, Session*>::iterator iter = activeSession.find(sid);
    if (iter != activeSession.end())
    {
        sess = iter->second;
    } else {
    }
    pthread_mutex_unlock(&g_session_mutex);
    return sess;
}

int releaseSession(int sid)
{
    printf("releaseSession beg -- sid:%d", sid);
    pthread_mutex_lock(&g_session_mutex);
    std::map<int, Session*>::iterator iter = activeSession.find(sid);
    if (iter != activeSession.end()) {
        delete iter->second;
        activeSession.erase(iter);
    } else {
    }
    printf("releaseSession end");
    pthread_mutex_unlock(&g_session_mutex);
    return 0;
}

Session*  raiseSession1(int sid,int ip,short port,string root_dir,bool egross)
{
    printf("raiseSession1 begin");
    //const char *dir = root_dir.data();//获取本地地址安卓
    const char *dir = "silk";
    ip = ntohl(ip);

    //绑定地址
    sockaddr_in remote;
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(ip);
    remote.sin_port = htons(port);
    printf(">>> raiseSession - sid:%d, ip:port:%s:%d, dir:%s", sid,inet_ntoa(remote.sin_addr), ntohs(remote.sin_port), dir);

    Session *sess = getSessionById(sid); //通过sid从容器中获取会话
    if (NULL == sess)
    {
        if (egross) //是否点对点
        {
            sess = new Session(ip, port, dir, sid);
        } else {
            sess = new Session(ip, port, dir);
        }
        pthread_mutex_lock(&g_session_mutex);
        activeSession[sid]=sess;
        pthread_mutex_unlock(&g_session_mutex);

    } else
    {
        printf("session already exist !!");
    }
    printf("raiseSession1 end %p", sess);

    return  sess;
}


int startHeartBeat(Session*  session, int ip, short port,int uin, int expired, int interval, int timeout)
{
    printf("startHeartBeat begin");
    g_report_uin = uin;//用户标识
    Session *sess = (Session *) session;
    ip = ntohl(ip);

    //设置地址端口等
    sockaddr_in remote;
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(ip);
    remote.sin_port = htons(port);

    printf(
        "startHeartBeat - ip:%s, port:%d, uid:%d, expired:%d, interval:%d, timeout:%d",
        inet_ntoa(remote.sin_addr), ntohs(remote.sin_port), uin, expired,
        interval, timeout);


    if (NULL != sess)
    {

        SessionHeartbeat heartBeat(ip, port, uin, expired, interval, timeout); // memory leak 内存泄漏???
        sess->standby(heartBeat);

        printf("startHeartBeat done");
        return 0;
    }
    return -1;
}


int startListen(Session* session, int handle)
{
    printf("startListen begin - session:%d, handle:%d", session, handle);
    Session *sess = (Session *) session;
    if (NULL != sess)
    {
        return sess->startPlayVoice(handle);
    }
    printf("startListen error");
    return -1;
}

SessionMsg Session::startPlayVoice(vl_uint32 handler)
{

    /*
  IncomingTransaction* incoming = findIncomingTransaction(handler);
  if(NULL == incoming) {
    LOGE("session start play failed, transaction not exist");
    return MSG_TRANS_NOT_EXIST;
  }

    vl_status ret = incoming->startTrack();
    if(VL_SUCCESS != ret) {
        LOGE("session start play failed, ret=%d", ret);
        return MSG_AUDDEV_ERROR;
    } else {
        return MSG_SUCCESS;
    }
    */
        return MSG_SUCCESS;
}

int createListen(Session* session, int ssrc)
{
    printf("createListen -- session:%d, ssrc:%d", session, ssrc);

    Session *sess = (Session *) session;//创建一个会话类
    if (NULL != sess) {
        int ret = sess->createIncomingTransaction(ssrc, false);//创建接收队列
        printf("create listen ret=%d", ret);
        return ret;
    }
    printf("createListen error");
    return 0;
}


int stopListen(int session, int handle)
{
    printf("stopListen - session:%d, handle:%d", session, handle);
    Session *sess = (Session *) session;
    if (NULL != sess) {
        return sess->stopPlayVoice(handle);
    }
    printf("stopListen error");
    return -1;
}

int releaseListen(int session, int handle,int waitMilSec)
{
    printf("releaseListen - session:%d, handle:%d, wait:%d", session, handle,waitMilSec);
    Session *sess = (Session *) session;
    if (NULL != sess)
    {
        return sess->releaseIncomingTransaction(handle, waitMilSec);
    }
    printf("releaseListen error");
    return -1;
}

void notifyMediaHeartbeatReceive(int i) {
    mediaCallback.onMediaHeartbeatRecv(i);

}
void incoming_trigger(int type, int send_id, int recv_id, int ssrc)
{
    printf() << "77777777777777777777777777777777777777777"<<send_id;
    //type:1 固定群聊  2 一对一  3 临时群聊
    //        * talkornot:1 说话 0 不说话
    pttServer->speaker = send_id;
    pttServer->speakStau=1;
    pttServer->sendSpeakStatus();

    Session *sess = getSessionById(Current_uin); //通过sid从容器中获取会话
    createListen(sess,ssrc);
    startListen(sess,ssrc);
    mediaCallback.onNewIncomingTransByJNI(type, send_id, recv_id, ssrc);
}

void initialCrashTrack() {

    return;
}

int OnLoad(void*reserved)
{
    printf("jni loading 20150703");
    StreamPlayScheduler::getInstance()->setObserver(&mediaCallback);
    StreamPlayScheduler::getInstance()->startWork();

    initialCrashTrack();
    return 0;
}

int createSpeakNetParam(Session *session, int ssrc, int srcId,
    int dstId, int sesstype, int netType, int netSignal)
{
    printf(
        "createSpeakNetParam  session:%d, ssrc:%d, srcId:%d, dstId:%d, sesstype:%d, netType:%d, netSignal:%d",
        session, ssrc, srcId, dstId, sesstype, netType, netSignal);

    Session *sess = (Session *) session;
    if (NULL != sess)
    {
        srcId = htonl(srcId);
        dstId = htonl(dstId);

        unsigned int report_net_type = gReportConvertNettype(netType);//获取网络类型 pc端直接设置为1  WiFi
        unsigned int report_os_type;

        report_os_type = 2;
        ExternData ext = { (vl_uint32) srcId, (vl_uint32) dstId,
                          (vl_uint32) sesstype, report_net_type, report_os_type, 0 };

        int len = sizeof(ext);
        int id = 0;

        TransactionOutputParam param;
        int ret = getTransactionOutputParam(param, ssrc, &ext, len, id, netType,
                                            netSignal);
        printf(
            "getParam - ret:%d;\nExternData - handle:%d, ssrc:%d, uid:%d, dstid:%d, sesstype:%d, netType:%d, netSignal:%d, len:%d, id:%d",
            ret, (int )sess, ssrc, srcId, dstId, sesstype, netType,
            netSignal, len, id);

        if (0 == ret) {
            return sess->createOutgoingTransaction(param);
        }
    }
    printf("createSpeakNetParam -- error!!!");
    return 0;
}

int getTransactionOutputParam(TransactionOutputParam &param, vl_uint32 ssrc,
                              void* extension, vl_size extensionLen, vl_uint16 extId,
                              vl_uint32 nettype, vl_uint32 netsignal) {

    param.format = AUDIO_FORMAT_SILK;

    if (1 == nettype) {
        param.bandtype = NARROW_BAND;
    } else if (2 == nettype) {
        param.bandtype = MEDIUM_BAND;
    } else if (3 == nettype) {
        param.bandtype = WIDE_BAND;
    } else {
        param.bandtype = MEDIUM_BAND;
    }
    if (netsignal < 0 || netsignal > 10) {
        netsignal = 5;
    }
    param.qulity = netsignal;
    param.ssrc = ssrc;
    param.dtx = VL_FALSE;
    param.fec = VL_TRUE;
    param.cbr = VL_TRUE;
    param.extension = extension;
    param.extensionLen = extensionLen;
    param.extId = extId;

    return 0;
}

int startSpeak(Session* session, int handle)
{
    printf("startSpeak - session:%d, handle:%d", session, handle);
    Session *sess = (Session *) session;
    if (NULL != sess) {
        SessionMsg ret = sess->startSpeak(handle);
        if (MSG_SUCCESS == ret)
        {
            return 0;
        } else {
            return -2;
        }
    }
    return -1;
}

int stopSpeak(Session* session, int handle)
{
    printf("stopSpeak - session:%d, handle:%d", session, handle);
    Session *sess = (Session *) session;
    if (NULL != sess)
    {
        SessionMsg ret = sess->stopSpeak(handle);
        if (MSG_SUCCESS == ret)
        {
            return 0;
        } else {
            return -2;
        }
    }
    return -1;
}


void playLocalFile(string filePath, int ssrc)
{
    const char *file = filePath.data();
    printf("playLocalFile - file:%s, ssrc:%d", file, ssrc);
    SilkFileStreamSource* silkFileSource = new SilkFileStreamSource(file, ssrc);
    vl_bool ret = false;
    if (silkFileSource->spsReadyForPlay())
    {
        ret = StreamPlayScheduler::getInstance()->delegate(silkFileSource,
                                                           VL_TRUE);
    }

    return ;
}


int stopPlayLocalFile(int ssrc)
{
    printf("stopPlayLocalFile - ssrc:%d", ssrc);
    vl_int32 ret = 0;
    if (0 == ssrc) {
        StreamPlayScheduler::getInstance()->terminatePlay();
    } else {
        StreamPlayScheduler::getInstance()->terminatePlay(ssrc);
    }
    return ret;
}


void onMiniteTick() {

    time_t _time;
    char buffer[40];
    struct tm* tm_info;
    time(&_time);
    tm_info = localtime(&_time);
    strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", tm_info);
    printf("receive a minite tick %s", buffer);
    EndPointManager::getInstance()->rtcKeepAlive();
}
