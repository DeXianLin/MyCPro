#ifndef __AUDIO_DEV_MGR_HPP__
#define __AUDIO_DEV_MGR_HPP__
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include "vl_types.h"
#include "AudioInfo.hpp"
#include "UnitCircleBuffer.hpp"
#include "Time.hpp"
#include <limits.h>
//#include <Windows.h>

#define MAXRECBUFFER  10
#define RECBUFFERSIZE 1200
/* 尽量多一些缓冲存放接受语音的数据 */
#define NUM_BUFFERS         (2)
#define MAX_VOLUME          (32)
#define AUDDEV_INVALID_ID   (0)
#define PLAY_NUM_BUFFERS    (250 * 4)

extern int OS_API_VERSION;
#define OPENSL_CUT_VER      (19)


typedef struct {
    vl_int16 init_vol;
} aud_dev_setting;

/**
 * 录音参数(每次启动录音时传入)
 */
typedef struct RecordParameter {
    PCMInfo pcmInfo;
} RecordParameter;


/**
 * 播放参数(每次启动播放流时传入)
 */
typedef struct PlayParameter {
    PCMInfo pcmInfo;
    /* 标志从feeder中获取不到新的pcm时，是否自动释放播放设备，用于文件播放 */
    vl_bool   auto_release;
    /* 指定从那个声音通道播放, 设置为 <0 时候，默认使用music通道 */

    vl_int32 streamType;

} PlayParameter;

/**
 * 播放回调接口
 */
class PCMFeeder {
public:
    virtual ~PCMFeeder() {};
    virtual vl_status feedPcm(PCMInfo* pcmInfo, UnitCircleBuffer* circleBuffer) = 0;
};
/**
 * 录音回调接口
 */
class PCMConsumer {
public:
    virtual ~PCMConsumer() {};
    virtual vl_status consumePcm(PCMInfo* param, const vl_uint8* samples, vl_size* sample_count) = 0;
};


/**
 * 负责管理声音设备，单例模式
 * 不同群之间共享一个媒体管理实例，但每个群可以有部分不同的语音配置。需要统一的参数如下：
 * 同一时间，只能有一个声音播放，也只能有一个录音。
 * 
 * 关于声音设备，不同群主要靠不同的语音Filter做处理
 * 播放声音时候，参数有一个选项，在取不到数据时，是终止播放还是播放静音缓冲。在播放文件时，选择终止；播放实时流，选择播放静音。
 */
class AudioDevMgr {
public:
    /* 获取媒体设备管理实例 */
    static AudioDevMgr* getInstance();
    /* 请求播放 */
    vl_status aquirePlayer(const PlayParameter& param, PCMFeeder* feeder, int* handler, UnitCircleBuffer* circleBuf);
    /* 改变播放参数 */
    vl_status reconfigPlayer(const PlayParameter& param, int handler, UnitCircleBuffer* circleBuf);
    /* 加大音量 */
    vl_status incVolume();
    /* 减少音量 */
    vl_status decVolume();
    /* 设置音量, 音量值范围为32 */
    vl_status setVolume(vl_int16 volume);
    /* 暂停播放 */
    vl_status pausePlay(int handler);
    /* 恢复播放 */
    vl_status resumePlay(int handler);
    /*开始播放, 传入aquire的id */
    vl_status startPlay(int handler);
    /* 停止播放 */
    vl_status stopPlay(int handler, vl_bool wait);
    /* 释放播放设备 */
    vl_status releasePlayer(int handler);
    /*
   * 请求录音
   * waitFlush 用于opensl在stop后回调线程会残留部分数据开关，waitFlush表示等待缓冲数据结束后再释放
   */
    vl_status aquireRecorder(const RecordParameter& param, PCMConsumer* consumer, int* handler);
  /* 更新录音参数 */
    vl_status reconfigRecorder(const RecordParameter& param, int handler);
    /* 开始录音 */
    vl_status startRecord(int handler);
    /* 停止录音 */
    vl_status stopRecord(int handler);
    /* 释放录音设备 */
    vl_status releaseRecorder(int handler);
    /* 录音异步关闭，用于获取录音设备状态, 注意，该函数线程安全 */
    vl_bool isRecording() const;

//private:
    /* 初始化播放设备 */
    bool initialPlayer(const PlayParameter&);
    /* 初始化录音设备 */
    bool initialRecorder(const RecordParameter&);

    // opensl 录音设备具体申请释放
    vl_status recordDevInitial(const RecordParameter&);
    vl_status recordDevRelease();

    /* singleton，获取实例时不指定初始化参数 */
    static AudioDevMgr * instance;
    AudioDevMgr();
    ~AudioDevMgr();
    /* --------------------私有成员函数-------------------- */
    /* 重置,只在类的构造函数调用一次 */
    void reset();
    /* 初始化，对象在重复使用时候，可调用该函数 */
    void initial();
    /* 对象在重复使用前，由于参数配置不同，可调用该函数后调用初始化 */
    void deinitial();

    /* 互斥获取设备id */
    int getPlayer()
    {
        int id = AUDDEV_INVALID_ID;
        if(0 == pthread_mutex_trylock(&playerLock))
        {
            if(VL_FALSE == playerOccupied) {
                playerId ++;
                if(AUDDEV_INVALID_ID == playerId) {
                    playerId ++;
                }

                id = playerId;
                playerOccupied = VL_TRUE;
            }
            pthread_mutex_unlock(&playerLock);
        } else {
            printf("AudioDevMgr aquire player id, get lock failed");
        }
        return id;
    }

    void putPlayer(int handler)
    {
        //printf("AudioDevMgr put player");
        pthread_mutex_lock(&playerLock);
        printf("AudioDevMgr put player locked");
        if(playerId == handler) {
            playerOccupied = VL_FALSE;
        }
        pthread_mutex_unlock(&playerLock);
    }

    int getRecorder() {
        int id = AUDDEV_INVALID_ID;
        vl_bool occuped = VL_TRUE;

        if(0 == pthread_mutex_trylock(&recordLock)) {
            if(VL_FALSE == recordOccupied) {
                occuped = VL_FALSE;
                recordId ++;
                if(AUDDEV_INVALID_ID == recordId) {
                    recordId ++;
                }

                id = recordId;
                recordOccupied = VL_TRUE;
            } else {
                printf("AudioDevMgr get recorder but occuppied !!!");
            }
            pthread_mutex_unlock(&recordLock);
        } else {
            printf("AudioDevMgr aquire recorder id, get lock failed");
        }

        if(VL_FALSE == occuped) {
            printf("AudioDevMgr get recorder id=%d", id);
        }
    return id;
    }
    void putRecorder(int handler) {
        pthread_mutex_lock(&recordLock);
        if(recordId == handler) {
            printf("AudioDevMgr put recorder innerid=%d id=%d", recordId , handler);
            recordOccupied = VL_FALSE;
        }
        pthread_mutex_unlock(&recordLock);
    }

    /* opensl 可用 */
    vl_bool             isReady;

    /* 内部播放缓冲区 */
    vl_bool             playing;
    unsigned            playerBufferSize;
    //  vl_uint8           *playerBuffer[NUM_BUFFERS];
    vl_uint8*           noiseBuffer;
    UnitCircleBuffer*   playerBuffer;
    int                 playerBufIdx;
    PCMFeeder          *feeder;
    PlayParameter       playerParam;
    int                 playerId;
    pthread_mutex_t     playerLock;
    vl_bool             playerOccupied;
    vl_bool             playerProc;

    /* 内部录音缓冲区 */
    vl_bool             recording;
    unsigned            recordBufferSize;
    vl_uint8           *recordBuffer[NUM_BUFFERS];
    int                 recordBufIdx;
    PCMConsumer        *consumer;
    RecordParameter     recordParam;
    int                 recordId;
    pthread_mutex_t     recordLock;
    vl_bool             recordOccupied;
    vl_bool             recordProc;

    /* Opensl 录音机制问题, android 4.4 有释放有死锁，且回调机制录音会丢失尾部语音 */
    vl_uint64           recordMarkStartTS;
    vl_uint64           recordMarkStopTS;
    vl_size             recordMarkSavedMS;
    vl_size             recordWouldDropMS;
    vl_size             recordDropedMS;


    //zjw add
    //zjw add
    void bqPlayerCallback(void * auddev);
    WAVEHDR *m_pWaveHeaderPlay;
    HWAVEOUT m_hPlay;
    HWAVEIN m_hRecord;
    HWAVEOUT hWaveOut = NULL;
    WAVEHDR *pWaveHeader;

    int m_IsAllocated;
    WAVEHDR* waveBlocks;
    int waveCurrentBlock;
    volatile unsigned int waveFreeBlockCount;


    int totalpcmsize;
    WAVEFORMATEX m_WaveFormatEx;
    LPWAVEHDR m_RecHead[10];//不确定


    bool play_audio(AudioDevMgr * pAuddev);
    WAVEHDR* allocateBlocks(int size,int count,vl_uint8* buffera);
    void writeAudio(AudioDevMgr * pAuddev);

    LPWAVEHDR  CreateWaveHeader();
    void PreCreateHeader();


    /* 标记录音开始 */
    void recordMarkStart()
    {
        recordMarkStartTS = getTimeStampMS();
        recordMarkStopTS = ULLONG_MAX;
        recordMarkSavedMS = 0;
        recordDropedMS = 0;
        printf("AudioDevMgr solv: id=%d, start ts=%llu", recordId, recordMarkStartTS);
    }

    /* 标记录音将结束 */
    void recordMarkStop()
    {
        recordMarkStopTS = getTimeStampMS();
        printf("AudioDevMgr solv: id=%d, mark stop ts=%llu", recordId, recordMarkStopTS);
    }

    /* 触发录音正式停止事件 */
    void recordUpdateStopTS()
    {
        printf("AudioDevMgr solv: record stoped id=%d, total record duration=%d !!!", recordId, recordMarkSavedMS);
        recordWouldDropMS = 0;
        recordMarkStartTS = 0L;
        recordMarkSavedMS = 0;
    }

    /* 记录语音数据回调，若语音为有效语音，返回true，否则返回false */
    bool onRecordCallback(vl_size duration) {
        if(recordDropedMS < recordWouldDropMS) {
            recordDropedMS += duration;
            //printf("AudioDevMgr solv: id=%d, record callback invalid pcm, duration=%d wouldDrop=%d !!!", recordId, recordDropedMS, recordWouldDropMS);
            return false;
        } else {
            recordMarkSavedMS += duration;
            //printf("AudioDevMgr solv: id=%d, record callback valid pcm, duration=%d !!!", recordId, recordMarkSavedMS);
            return true;
        }
    }

    /* 判断是否真正停止录音 */
    bool timeToStopRecord()
    {
        /* 从未启动过 */
        if(0L == recordMarkStartTS)
        {
            printf("AudioDevMgr solv: id=%d, time to stop, not started", recordId);
            return true;
        }

        /* 缓冲数据已经足够，结束标签 */
        if(recordMarkSavedMS >= (recordMarkStopTS - recordMarkStartTS))
        {
            return true;
        }

        return false;
    }

};

#endif
