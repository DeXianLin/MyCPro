#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
//#include <malloc.h>
#include "StreamPlayScheduler.hpp"
#include "vl_const.h"
//#include "vl_log.h"
#include "AudioDevMgr.hpp"
#include "Time.hpp"

#define _MALLOC malloc
#define _FREE free
#define _MEMSET memset

#define BLOCK_SIZE 50//4608
#define BLOCK_COUNT 20

static CRITICAL_SECTION waveCriticalSection;
static CRITICAL_SECTION playerLockW;
static CRITICAL_SECTION recordLockW;

AudioDevMgr* AudioDevMgr::instance = NULL;

static void generateNextFrame(vl_uint8* pcmData, vl_size length) {
    memset(pcmData, 0, length);
}

void writetopcm(LPSTR lpData,DWORD dwBufferLength)
{
    //将音频写入到文件中
    FILE* fp=fopen("record.pcm","ab+");
    if(fp==NULL)
    {
        printf("fopen error,%d",__LINE__);
    }
    fwrite(lpData,dwBufferLength,1,fp);
    fclose(fp);
    //jams
}
WAVEHDR* AudioDevMgr::allocateBlocks(int size, int count,  vl_uint8*   buffera)
{
    char* buffer;
    int i;

    DWORD totalBufferSize = (size + sizeof(WAVEHDR)) * count;

    /*
    * allocate memory for the entire set in one go一次性为整个集合分配内存
    */
    if ((buffer = (char*)HeapAlloc(
             GetProcessHeap(),
             HEAP_ZERO_MEMORY,
             totalBufferSize
             )) == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        ExitProcess(1);
    }

    /*
    * and set up the pointers to each bit  并设置指向每一位的指针
    */
    waveBlocks = (WAVEHDR*)buffer;
    buffer += sizeof(WAVEHDR)* count;
    for (i = 0; i < count; i++) {
        waveBlocks[i].dwBufferLength = size;
        waveBlocks[i].lpData = (LPSTR)buffer;
        waveBlocks[i].dwUser=0;
        buffer += size;
    }

    return waveBlocks;
}

bool AudioDevMgr::play_audio(AudioDevMgr * pAuddev)
{
    //printf("play_audio==============");
    waveOutUnprepareHeader(pAuddev->m_hPlay,pAuddev->m_pWaveHeaderPlay,sizeof(WAVEHDR));//jams
    pAuddev->m_pWaveHeaderPlay->dwBufferLength = pAuddev->playerBufferSize;
    pAuddev->m_pWaveHeaderPlay->lpData = (LPSTR)pAuddev->noiseBuffer;
    if (::waveOutPrepareHeader(pAuddev->m_hPlay, pAuddev->m_pWaveHeaderPlay, sizeof(WAVEHDR)))
    {
        printf("AudioDevMgr waveOutPrepareHeader failed");
        return false;
    }

    if (::waveOutWrite(pAuddev->m_hPlay, pAuddev->m_pWaveHeaderPlay, sizeof(WAVEHDR)))
    {
        printf("AudioDevMgr waveOutWrite failed");
        return false;
    }

    return true;
}



void AudioDevMgr::writeAudio(AudioDevMgr * pAuddev)
{
    //printf() << "writeAudio=========";
#if 1
    LPWAVEHDR current_wavehdr;
    int remain;
    DWORD dwUsr;
    current_wavehdr = &waveBlocks[waveCurrentBlock];

    while (pAuddev->playerBufferSize > 0) {
        /*
        * first make sure the header we're going to use is unprepared
        */
        if (current_wavehdr->dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(pAuddev->m_hPlay, current_wavehdr, sizeof(WAVEHDR));

        if (pAuddev->playerBufferSize < (int)(BLOCK_SIZE - current_wavehdr->dwUser)) {
            memcpy(current_wavehdr->lpData + current_wavehdr->dwUser, pAuddev->noiseBuffer, pAuddev->playerBufferSize);
            current_wavehdr->dwUser += pAuddev->playerBufferSize;
            break;
        }

        remain = BLOCK_SIZE - current_wavehdr->dwUser;
        memcpy(current_wavehdr->lpData + current_wavehdr->dwUser, pAuddev->noiseBuffer, remain);

        pAuddev->playerBufferSize -= remain;
        pAuddev->noiseBuffer += remain;
        current_wavehdr->dwBufferLength = BLOCK_SIZE;

        waveOutPrepareHeader(pAuddev->m_hPlay, current_wavehdr, sizeof(WAVEHDR));
        waveOutWrite(pAuddev->m_hPlay, current_wavehdr, sizeof(WAVEHDR));

        waveFreeBlockCount--;

        /*
        * wait for a block to become free
        */
        while (!waveFreeBlockCount)
            Sleep(10);

        /*
        * point to the next block
        */
        waveCurrentBlock++;
        waveCurrentBlock %= BLOCK_COUNT;

        current_wavehdr = &pAuddev->waveBlocks[waveCurrentBlock];
        current_wavehdr->dwUser = 0;
    }
#endif
}
static void freeBlocks(WAVEHDR* blockArray)
{
    /*
    * and this is why allocateBlocks works the way it does 这就是为什么allocateBlocks 的工作方式
    */
    HeapFree(GetProcessHeap(), 0, blockArray);
}

//Callback function
void CALLBACK MicCallback12(HWAVEIN hwi,
                            UINT uMsg,
                            DWORD dwInstance,
                            DWORD dwParam1,
                            DWORD dwParam2)
{

    AudioDevMgr * pAuddev = (AudioDevMgr *) dwInstance;
    if(uMsg == WIM_DATA)
    {
        WAVEHDR *pwh;

        pwh=(WAVEHDR *)dwParam1;
    }
}

void bqPlayerCallback(void * auddev)
{
    if(auddev != NULL)
    {
        AudioDevMgr * pAuddev = (AudioDevMgr *) auddev;
        pAuddev->playerProc = VL_TRUE;

        if(VL_TRUE == pAuddev->playing)
        {
            if(NULL != pAuddev->feeder)
            {
                //printf() << "bqPlayerCallback======================";
                pAuddev->feeder->feedPcm(&pAuddev->playerParam.pcmInfo, pAuddev->playerBuffer);
            }
            vl_int16* samples = NULL;
            if(NULL != pAuddev->playerBuffer)
            {
                if(VL_SUCCESS != pAuddev->playerBuffer->getRange(&samples, pAuddev->playerBufferSize / 2))
                {
                    if(pAuddev->playerBuffer->getLength() > 0 && VL_FALSE == pAuddev->playerBuffer->hasMoreData())
                    {

                        /* 环形缓冲不会再有新数据输入，取出残留的数据 */
                        vl_size left = pAuddev->playerBuffer->getLength();
                        printf("AudioDevMgr player has no more data to feed, dump left %d\r\n ", left);
                        pAuddev->playerBuffer->getRange(&samples, left);
                        memset(pAuddev->noiseBuffer, 0, pAuddev->playerBufferSize);
                        memcpy((void*)pAuddev->noiseBuffer, samples, left);
                        samples = (vl_int16*)pAuddev->noiseBuffer;

                    }// has more false
                    else {
                        /* 无数据可写入播放缓冲，播放静音 */
                        printf("AudioDevMgr player has no enough data in circle buffer, expect %d, left %d !!!\r\n",
                               pAuddev->playerBufferSize / 2, pAuddev->playerBuffer->getLength());
                        generateNextFrame(pAuddev->noiseBuffer, pAuddev->playerBufferSize);
                        samples = (vl_int16*)pAuddev->noiseBuffer;
                    }
                }//get rangle false
                else {

                    memcpy((void*)pAuddev->noiseBuffer, samples, pAuddev->playerBufferSize);
                }

#ifndef USE_BLOCK_PLAY
                pAuddev->play_audio(pAuddev);
#else
                pAuddev->writeAudio(pAuddev);
#endif//jams

            }//playerBuffer!=null
        }//playing
        pAuddev->playerProc = VL_FALSE;
    }

}

void bqRecorderCallback( void * auddev,LPSTR lpData) {
    if(auddev != NULL)
    {
        AudioDevMgr * pAuddev = (AudioDevMgr *) auddev;
        pAuddev->recordProc = VL_TRUE;

        if(VL_TRUE == pAuddev->recording)
        {
            if(NULL != pAuddev->consumer)
            {
                vl_size sample_size = pAuddev->recordBufferSize * 8 / pAuddev->recordParam.pcmInfo.bits_per_sample;
                vl_size durationMS = sample_size * 1000 / pAuddev->recordParam.pcmInfo.sample_rate;
                if(pAuddev->onRecordCallback(durationMS))
                {
                    vl_uint8 * output_ptr = (vl_uint8 *)lpData;//pAuddev->recordBuffer[pAuddev->recordBufIdx];//jams

                    //将音频写入到文件中 output_ptr
                    pAuddev->consumer->consumePcm(&pAuddev->recordParam.pcmInfo, output_ptr  , &sample_size);
                }//onRecordCallback
            }//consumer
            if(false == pAuddev->timeToStopRecord())
            {
                pAuddev->recordBufIdx = (pAuddev->recordBufIdx + 1) % NUM_BUFFERS;

            }//timeToStopRecord false
            else {
                pAuddev->recordUpdateStopTS();
                if(VL_TRUE == pAuddev->recording)
                {
                    pAuddev->recording = VL_FALSE;

                    StreamPlayScheduler::getInstance()->wakeup();
                }//recording
                else {
                    printf("AudioDevMgr opensl recorder is not recoding now\r\n");
                }
            }//timeToStopRecord true
        }//recording
        pAuddev->recordProc = VL_FALSE;
    }
}

static DWORD_PTR CALLBACK MicCallback(  //消息回掉函数
    HWAVEIN hWaveIn,
    UINT  uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2)
{
    AudioDevMgr * pAuddev = (AudioDevMgr *) dwInstance;
    WAVEHDR* lpHdr =  ((WAVEHDR*)dwParam1);//->lpData;

    if(pAuddev->recording==FALSE) //如果当前不在采集状态
        return 0;//FALSE;
    switch(uMsg)
    {
    case WIM_OPEN:
        //do something

        break;
        //
    case WIM_DATA:
    {
        if(lpHdr->dwBytesRecorded==0 || lpHdr==NULL)
            return ERROR_SUCCESS;//ERROR_SUCCESS;

        //使采集过程，知道此buffer已经沾满，不能再填充

        bqRecorderCallback(pAuddev,lpHdr->lpData);
        ::waveInUnprepareHeader(pAuddev->m_hRecord, lpHdr, sizeof(WAVEHDR));

        if(pAuddev->recording)
        {
            //重新将buffer恢复到准备填充状态 //准备一个bufrer给输入设备
            ::waveInPrepareHeader(pAuddev->m_hRecord, lpHdr, sizeof(WAVEHDR));
            ::waveInAddBuffer(pAuddev->m_hRecord, lpHdr, sizeof(WAVEHDR));
        }
    }
    break;

    case WIM_CLOSE:
        //do something
        break;
    default:
        break;
    }
    return 0;
}

static void CALLBACK waveOutProc( HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 )
{
    AudioDevMgr* pThis=(AudioDevMgr*)dwInstance;
    if(WOM_DONE == uMsg)    //播放完成
    {

#ifdef USE_BLOCK_PLAY
        pThis->waveFreeBlockCount++;
#endif
        bqPlayerCallback(pThis);
    }
    return ;
}



void AudioDevMgr::reset()  {
    isReady = VL_FALSE;
    isReady = VL_FALSE;

    playing = VL_FALSE;
    playerId = AUDDEV_INVALID_ID;
    playerOccupied = VL_FALSE;

    recording = VL_FALSE;
    recordId = AUDDEV_INVALID_ID;
    recordOccupied = VL_FALSE;

    consumer = NULL;
    feeder = NULL;

    recordWouldDropMS = 0;
    recordMarkSavedMS = 0;
    playerBuffer = NULL;
    noiseBuffer = NULL;
    playerBufIdx = 0;

    _MEMSET(&playerParam, 0, sizeof(playerParam));


    recordBufferSize = 0;
    recordBufIdx = 0;
    _MEMSET(&recordParam, 0, sizeof(recordParam));
    printf("AudioDevMgr Audio device reset done\r\n");
}
//WinAudioPlay::



void AudioDevMgr::initial()
{
    RecordParameter recordParam;
    recordParam.pcmInfo.sample_cnt = 160;//160
    recordParam.pcmInfo.sample_rate = 8000;
    recordParam.pcmInfo.channel_cnt = 1;
    recordParam.pcmInfo.bits_per_sample = 16;//jams  16

    if(false == initialRecorder(recordParam)) {
        printf("AudioDevMgr initial recorder failed\r\n");
    }

    PlayParameter playerParam;
    playerParam.pcmInfo.sample_cnt = 160;//jams 160;//jams 160; 5120
    playerParam.pcmInfo.sample_rate = 8000;
    playerParam.pcmInfo.channel_cnt = 1;
    playerParam.pcmInfo.bits_per_sample = 16;//16
    playerParam.auto_release = VL_FALSE;

    if(false == initialPlayer(playerParam)) {
        printf("AudioDevMgr initial player failed\r\n");
    }
    pthread_mutex_init(&playerLock, NULL);
    pthread_mutex_init(&recordLock, NULL);
    isReady = VL_TRUE;

    InitializeCriticalSection(&waveCriticalSection);
    InitializeCriticalSection(&playerLockW);
    InitializeCriticalSection(&recordLockW);

#ifdef USE_BLOCK_PLAY
    // ZeroMemory(&wfx,sizeof(WAVEFORMATEX));
    waveBlocks          = NULL;
    waveFreeBlockCount = 0;
    waveCurrentBlock   = 0;

    waveBlocks = allocateBlocks(BLOCK_SIZE, BLOCK_COUNT,noiseBuffer);
    waveFreeBlockCount = BLOCK_COUNT;
    waveCurrentBlock = 0;
#else
    m_pWaveHeaderPlay = new WAVEHDR;
    memset(m_pWaveHeaderPlay, 0, sizeof(WAVEHDR));
    m_pWaveHeaderPlay->dwLoops = 1;
    m_pWaveHeaderPlay->dwFlags =0;

#endif
    //打开音频输出设备
    WAVEFORMATEX    wfx;
    ZeroMemory(&wfx,sizeof(WAVEFORMATEX));

    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 8000;//44100L;
    wfx.wBitsPerSample = 16;
    wfx.cbSize = 0;
    wfx.nBlockAlign = wfx.wBitsPerSample * wfx.nChannels / 8;
    wfx.nAvgBytesPerSec = wfx.nChannels * wfx.nSamplesPerSec * wfx.wBitsPerSample / 8;

    if(::waveOutOpen (0,0,&wfx,0,0,WAVE_FORMAT_QUERY))
    {
        printf("AudioDevMgr waveOutOpen failed\r\n");
        return ;
    }

    if (::waveOutOpen(&m_hPlay, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION))
    {
        printf("AudioDevMgr waveOutOpen failed\r\n");
        return ;
    }

    //::waveOutSetVolume(m_hPlay,0xAAAAAAAA);
    printf("AudioDevMgr Audio device initial done");
    return;
}

void AudioDevMgr::deinitial() {
    isReady = VL_FALSE;
    /* release player */
    playerBuffer = NULL;
    if(NULL != noiseBuffer) {
        free(noiseBuffer);
        noiseBuffer = NULL;
    }
    playerBufIdx = 0;

#ifdef USE_BLOCK_PLAY
    for(int i = 0; i < waveFreeBlockCount; i++)
    {
        if(waveBlocks[i].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(m_hPlay, &waveBlocks[i], sizeof(WAVEHDR));
    }
    freeBlocks(waveBlocks);
#else
    delete m_pWaveHeaderPlay;
    m_pWaveHeaderPlay=NULL;
    for(int i=0;i<MAXRECBUFFER;i++)
    {
        delete	m_RecHead[i]->lpData;
        m_RecHead[i]->lpData=NULL;
        delete m_RecHead[i];
        m_RecHead[i]=NULL;
    }
#endif

    waveOutClose(m_hPlay);
    DeleteCriticalSection(&waveCriticalSection);
    DeleteCriticalSection(&playerLockW);
    DeleteCriticalSection(&recordLockW);
    printf("AudioDevMgr Audio device deinitial done\r\n");
}

AudioDevMgr::AudioDevMgr() {
    /* 重置所有成员为未初始化状态 */
    reset();
    /* 初始化OpenSL engine */
    initial();
}

AudioDevMgr::~AudioDevMgr()
{
    printf("AudioDevMgr::~AudioDevMgr");
    /* 释放播放设备 */
    releasePlayer(playerId);
    /* 试图释放录音设备 */
    releaseRecorder(recordId);
    /* 释放OpenSL engine */
    deinitial();
}

AudioDevMgr* AudioDevMgr::getInstance() {
    if(instance == NULL) {
        instance = new AudioDevMgr();
    }
    return instance;
}



vl_status AudioDevMgr::incVolume() {
    return VL_SUCCESS;
}

vl_status AudioDevMgr::decVolume() {
    return VL_SUCCESS;
}

vl_status AudioDevMgr::setVolume(vl_int16 volume) {
    return VL_SUCCESS;
}

vl_status AudioDevMgr::pausePlay(int handler) {

    if(handler != playerId) {
        return VL_ERR_AUDDEV_ID;
    }
    return VL_SUCCESS;
}

vl_status AudioDevMgr::resumePlay(int handler) {

    if(handler != playerId) {
        return VL_ERR_AUDDEV_ID;
    }
    return VL_SUCCESS;
}
vl_status AudioDevMgr::aquirePlayer(const PlayParameter& param, PCMFeeder* feeder, int* handler, UnitCircleBuffer* circleBuf)
{
    printf("aquirePlayer=========");
    vl_status status = VL_SUCCESS;
    *handler = AUDDEV_INVALID_ID;

    if(VL_FALSE == isReady)
    {
        return VL_ERR_AUDDEV_INITAL;
    }

    *handler = getPlayer();

    if(AUDDEV_INVALID_ID == *handler)
    {
        printf("AudioDevMgr aquire player failed, device occupied !!!\r\n");
        return VL_ERR_AUDDEV_REC_OCCUPIED;
    }
    this->playerParam = param;
    this->feeder = feeder;

    do {
        playerBufferSize = playerParam.pcmInfo.sample_cnt
                           * playerParam.pcmInfo.channel_cnt
                           * playerParam.pcmInfo.bits_per_sample / 8;


         playerBufferSize=3200;//jams 3200 2048 2048  //2021 3 30zjw



        playerBuffer = circleBuf;
        if(circleBuf->getCapacity() == 0)
        {
            circleBuf->init(playerBufferSize * PLAY_NUM_BUFFERS);
        }

        noiseBuffer = (vl_uint8*)_MALLOC(playerBufferSize*100);//jams
        memset(noiseBuffer, 0, playerBufferSize);

        printf("AudioDevMgr Audio device aquire player done !!!\r\n");
    } while(0);
    return status;
}

vl_status AudioDevMgr::reconfigPlayer(const PlayParameter& param, int handler, UnitCircleBuffer* circleBuf) {
    printf("reconfigPlayer qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq");
    vl_status status;
    if(handler != playerId) {
        return VL_ERR_AUDDEV_ID;
    }

    PCMInfo& oldInfo = this->playerParam.pcmInfo;
    const PCMInfo& newInfo = param.pcmInfo;

    if(oldInfo == newInfo) {
        return VL_TRUE;
    }

    if(VL_TRUE == playing) {
        do {
            status = stopPlay(handler, VL_FALSE);
            if(VL_SUCCESS != status) {
                printf("AudioDevMgr opensl reconfig player stop play failed\r\n");
                break;
            }
            PCMFeeder* oldFeader = feeder;
            int tempHandler;

            status = releasePlayer(handler);
            if(VL_SUCCESS != status){
                printf("AudioDevMgr opensl reconfig player release player failed\r\n");
                break;
            }
            status = aquirePlayer(param, oldFeader, &tempHandler, circleBuf);
            if(VL_SUCCESS != status) {
                printf("AudioDevMgr opensl reconfig player aquire failed\r\n");
                break;
            }
            playerId = handler;

            status = startPlay(handler);
            if(status != VL_SUCCESS) {
                printf("AudioDevMgr opensl reconfig player play failed\r\n");
                break;
            }

        }while(0);
    }
    else {
        do {
            PCMFeeder* oldFeader = feeder;
            int tempHandler;

            status = releasePlayer(handler);
            if(VL_SUCCESS != status){
                printf("AudioDevMgr opensl reconfig player release player failed\r\n");
                break;
            }

            status = aquirePlayer(param, oldFeader, &tempHandler, circleBuf);
            if(VL_SUCCESS != status) {
                printf("AudioDevMgr opensl reconfig player aquire failed\r\n");
                break;
            }
            playerId = handler;

        } while(0);
    }
    return VL_SUCCESS;
}
bool AudioDevMgr::initialPlayer(const PlayParameter& param) {

    totalpcmsize=0;//jams
    playerBufIdx = 0;
    playerParam = param;

    return true;//   return VL_SUCCESS;
}
vl_status AudioDevMgr::startPlay(int handler)
{

    vl_status status = VL_SUCCESS;
    printf("AudioDevMgr start play\r\n");
    if(handler != playerId) {
        printf("AudioDevMgr VL_ERR_AUDDEV_ID failed");
        return VL_ERR_AUDDEV_ID;
    }

    printf("[MUTEX] LOCK %s %p",__FUNCTION__,&playerLock);
    pthread_mutex_lock(&playerLock);
    EnterCriticalSection(&playerLockW);
    waveOutRestart(m_hPlay);

    if(VL_TRUE == playing) {
        printf("AudioDevMgr opensl start play: already playing\r\n");
        status = VL_SUCCESS;
    } else {
        do {
            playing = VL_TRUE;

            vl_int16* samples;
            if(NULL != feeder && NULL != playerBuffer)
            {
                feeder->feedPcm(&playerParam.pcmInfo, playerBuffer);
                if(VL_SUCCESS == playerBuffer->getRange(&samples, playerBufferSize / 2))
                {
                    memcpy(noiseBuffer,samples,playerBufferSize);//jams

#ifndef USE_BLOCK_PLAY
                    play_audio(this);//playerBufferSize
#else
                    writeAudio(this);
#endif
                    printf("AudioDevMgr opensl start play enqueue from ioq\r\n");
                } else {
                    generateNextFrame(noiseBuffer, playerBufferSize);
                    printf("AudioDevMgr opensl start play enqueue but noise buffer because no bufferd audio data\r\n");
#ifndef USE_BLOCK_PLAY
                    play_audio(this);//playerBufferSize
#else
                    writeAudio(this);
#endif
                }

            } else {

                generateNextFrame(noiseBuffer, playerBufferSize);
#ifndef USE_BLOCK_PLAY
                play_audio(this);//playerBufferSize
#else
                writeAudio(this);
#endif
                printf("AudioDevMgr opensl start play enqueue but noise buffer feeder=%p and playerBuffer=%p\r\n", feeder, playerBuffer);
            }

            playerProc = VL_FALSE;
            status = VL_SUCCESS;
        } while(0);

    }
    pthread_mutex_unlock(&playerLock);
    LeaveCriticalSection(&playerLockW);
    printf("[MUTEX] UNLOCK %s %p\r\n",__FUNCTION__,&playerLock);
    return VL_SUCCESS;
}

vl_status AudioDevMgr::stopPlay(int handler, vl_bool wait)
{
    vl_status status = VL_SUCCESS;
    printf("AudioDevMgr stop play\r\n");
    pttServer->speakStau=0;
    pttServer->sendSpeakStatus();
    if(handler != playerId)
    {
        printf("AudioDevMgr %s stop play not valid handler\r\n", __FILE__);
        return VL_ERR_AUDDEV_ID;
    }

    EnterCriticalSection(&playerLockW);
#ifndef USE_BLOCK_PLAY
    if(m_pWaveHeaderPlay)
    {
        //printf("111111111111111111111");
        ::waveOutUnprepareHeader(m_hPlay, m_pWaveHeaderPlay, sizeof(WAVEHDR));//音频输出结束，清空buffer
    }
    //printf("222222222222222222222");
    Sleep(100);
    waveOutPause(m_hPlay);
#else
    if(pWaveHeader)
    {
        ::waveOutUnprepareHeader(m_hPlay, pWaveHeader, sizeof(WAVEHDR));//音频输出结束，清空buffer
    }

    waveOutReset(m_hPlay);

#endif
    if(VL_FALSE != playing)
    {
        playing = VL_FALSE;

    }
    LeaveCriticalSection(&playerLockW);
    return status;
}

vl_status AudioDevMgr::releasePlayer(int handler)
{
    //printf("releasePlayer====================");
    if(handler != playerId) {
        printf("AudioDevMgr release player not valid id\r\n");
        return VL_ERR_AUDDEV_ID;
    }
    putPlayer(handler);
    return VL_SUCCESS;
}

LPWAVEHDR  AudioDevMgr::CreateWaveHeader()
{
    LPWAVEHDR  m_wavlpHdr;
    m_wavlpHdr = new WAVEHDR;

    if(m_wavlpHdr==NULL)
    {
        //		m_RecodeLog.WriteString(TEXT("\n Unable to allocate the memory"));
        return NULL;
    }

    ZeroMemory(m_wavlpHdr, sizeof(WAVEHDR));
    char* lpByte = new char[RECBUFFERSIZE];

    if(lpByte==NULL)
    {
        //		m_RecodeLog.WriteString(TEXT("\n Unable to allocate the memory"));
        return NULL;
    }
    m_wavlpHdr->lpData =  lpByte;
    m_wavlpHdr->dwBufferLength = RECBUFFERSIZE;   // (m_WaveFormatEx.nBlockAlign*SOUNDSAMPLES);
    return m_wavlpHdr;

}
void AudioDevMgr::PreCreateHeader()
{
    for(int i=0;i<MAXRECBUFFER;i++)
    {
        m_RecHead[i]=CreateWaveHeader();
    }

    m_IsAllocated = 1;
}




// android 4.4， 录音设备之在初始化时候实例化一次
// 低于4.4版本，录音设备每次说话前申请，说话结束释放
bool AudioDevMgr::initialRecorder(const RecordParameter& param) {
    vl_status status = VL_ERR_AUDDEV_REC_INIT;

    if(VL_SUCCESS != recordDevInitial(param)) {
        return false;
    } else {
        this->recordParam = param;
    }

    recording = FALSE; //初始还未开始录制
    m_IsAllocated = 0;//初始还未分配buffer
    return true;
}

vl_status AudioDevMgr::aquireRecorder(const RecordParameter& param, PCMConsumer* consumer, int* handler) {
    vl_status status = VL_SUCCESS;
    *handler = AUDDEV_INVALID_ID;

    if(VL_FALSE == isReady) {
        printf("AudioDevMgr aquire recorder failed, device not ready !!!!\r\n");
        return VL_ERR_AUDDEV_INITAL;
    }

    //printf("AudioDevMgr opensl recorder param sr=%d, cc=%d, sc=%d, bps=%d\r\n",
           //param.pcmInfo.sample_rate, param.pcmInfo.channel_cnt, param.pcmInfo.sample_cnt, param.pcmInfo.bits_per_sample);


    *handler = getRecorder();
    if(AUDDEV_INVALID_ID == *handler) {
        printf("AudioDevMgr aquire recorder failed, device occupied !!!\r\n");
        return VL_ERR_AUDDEV_REC_OCCUPIED;
    }

    if(VL_SUCCESS != recordDevInitial(param)) {
        printf("AudioDevMgr aquireRecorder initial record device failed");

    }
    recordBufferSize = RECBUFFERSIZE;//jams param.pcmInfo.sample_cnt * param.pcmInfo.bits_per_sample * param.pcmInfo.channel_cnt / 8;

    if(VL_SUCCESS == status) {
        this->recordParam = param;
        this->consumer = consumer;
    } else {
        releaseRecorder(*handler);
        printf("AudioDevMgr aquire recorder failed, put handler back\r\n");
    }
    return status;
}

vl_status AudioDevMgr::reconfigRecorder(const RecordParameter& param, int handler) {
    vl_status status;

    if(handler != recordId) {
        return VL_ERR_AUDDEV_ID;
    }

    PCMInfo& oldInfo = this->recordParam.pcmInfo;
    const PCMInfo& newInfo = param.pcmInfo;

    if(oldInfo == newInfo) {
        return VL_TRUE;
    } else {
        if(VL_TRUE == recording) {
            do {
                status = stopRecord(handler);
                if(VL_SUCCESS != status) {
                    printf("AudioDevMgr reconfig recorder, stop failed\r\n");
                    break;
                }

                PCMConsumer* oldConsumer = this->consumer;
                int tempHandler;

                status = releaseRecorder(handler);
                if(VL_SUCCESS != status) {
                    printf("AudioDevMgr reconfig recorder, release failed");
                    break;
                }

                status = aquireRecorder(param, oldConsumer, &tempHandler);
                if(VL_SUCCESS != status) {
                    printf("AudioDevMgr reconfig recorder, aquire failed\r\n");
                    break;
                }
                this->recordId = handler;

                status = startRecord(handler);
                if(VL_SUCCESS != status) {
                    printf("AudioDevMgr reconfig recorder, start failed\r\n");
                    break;
                }
            } while(0);
        } else {
            do {
                PCMConsumer* oldConsumer = this->consumer;
                int tempHandler;

                status = releaseRecorder(handler);
                if(VL_SUCCESS != status) {
                    printf("AudioDevMgr reconfig recorder, release failed\r\n");
                    break;
                }

                status = aquireRecorder(param, oldConsumer, &tempHandler);
                if(VL_SUCCESS != status) {
                    printf("AudioDevMgr reconfig recorder, aquire failed\r\n");
                    break;
                }
                this->recordId = handler;
            } while(0);
        }
    }

    return status;
    return status;
}


vl_status AudioDevMgr::startRecord(int handler) {
    vl_status status = VL_SUCCESS;
    MMRESULT mmReturn=MMSYSERR_NOERROR;
    printf("AudioDevMgr audiodev start record ...\r\n");
    if(handler != recordId)
    {
        return VL_ERR_AUDDEV_ID;
    }
    //开启音频采集

    if(recording) //如果已经开启采集，则直接返回
    {
        return VL_ERR_AUDDEV_ID;//FALSE;
    }
    if(consumer == NULL)
    {
        printf("AudioDevMgr opensl can't start record, recordRecord is not ready\r\n");
        return VL_ERR_AUDDEV_REC_NOT_READY;
    }
    EnterCriticalSection(&recordLockW);

    if(VL_TRUE == recording)
    {
        printf("AudioDevMgr opensl is already recording\r\n");
        status = VL_SUCCESS;
    } else {
        PreCreateHeader();  //分配buffer
        memset(&m_WaveFormatEx, 0, sizeof(m_WaveFormatEx));
        m_WaveFormatEx.wFormatTag = WAVE_FORMAT_PCM;//声音格式为PCM
        m_WaveFormatEx.nChannels = 1;	//采样声道数，对于单声道音频设置为1，立体声设置为2
        m_WaveFormatEx.wBitsPerSample = 16;//采样比特  8bits/次
        m_WaveFormatEx.cbSize = 0;//一般为0
        m_WaveFormatEx.nSamplesPerSec = 8000; //采样率 16000 次/秒
        m_WaveFormatEx.nBlockAlign = 2; //一个块的大小，采样bit的字节数乘以声道数
        m_WaveFormatEx.nAvgBytesPerSec = 8000*2;//8000; //每秒的数据率，就是每秒能采集多少字节的数据
        MMRESULT  mmReturn = waveInOpen(&m_hRecord,WAVE_MAPPER,&m_WaveFormatEx,
                                       (DWORD_PTR)(MicCallback), DWORD_PTR(this), CALLBACK_FUNCTION);

        if(mmReturn != MMSYSERR_NOERROR ) //打开采集失败
        {
            printf("AudioDevMgr waveInOpen is  MMSYSERR_NOERROR failed %d\r\n",mmReturn);
            status=VL_ERR_CODEC_START;//FALSE;
        }
        if(mmReturn == MMSYSERR_NOERROR )
        {
            //将准备好的buffer提供给音频输入设备
            for(int i=0; i < MAXRECBUFFER ; i++)
            {
                //准备一个bufrer给输入设备
                mmReturn = ::waveInPrepareHeader(m_hRecord,m_RecHead[i], sizeof(WAVEHDR));
                //发送一个buffer给指定的输入设备，当buffer填满将会通知程序
                mmReturn = ::waveInAddBuffer(m_hRecord, m_RecHead[i], sizeof(WAVEHDR));
            }

        }

        //开启指定的输入采集设备
        mmReturn = ::waveInStart(m_hRecord);
        if(mmReturn!=MMSYSERR_NOERROR )  //开始采集失败
        {
            printf("AudioDevMgr audiodev start record MMSYSERR_NOERROR..failed %d.",mmReturn);
        }
        else
        {
            printf("AudioDevMgr audiodev start record ok.. %d.",mmReturn);
            recording = TRUE;
        }
        // mark record start time
        recordMarkStart();
    }
    LeaveCriticalSection(&recordLockW);
    return status;
}

vl_status AudioDevMgr::stopRecord(int handler) {

    vl_status status = VL_SUCCESS;


    if(handler != recordId) {
        printf("AudioDevMgr stopRecord stop record ...\r\n");
        return VL_ERR_AUDDEV_ID;
    }
    MMRESULT mmReturn = 0;

    //停止音频采集

    if(mmReturn == MMSYSERR_NOERROR )
    {
        //将准备好的buffer提供给音频输入设备
        for(int i=0; i < MAXRECBUFFER ; i++)
        {
            //准备一个bufrer给输入设备
            mmReturn = ::waveInUnprepareHeader(m_hRecord,m_RecHead[i], sizeof(WAVEHDR));
            //发送一个buffer给指定的输入设备，当buffer填满将会通知程序
            //	mmReturn = ::waveInAddBuffer(m_hRecord, m_RecHead[i], sizeof(WAVEHDR));
        }

    }

    mmReturn = ::waveInStop(m_hRecord);


    if(!mmReturn) //停止采集成功，立即重置设备,重置设备将会导致所有的buffer反馈给程序
    {
        recording = FALSE;
        mmReturn = ::waveInReset(m_hRecord);  //重置设备
    }
    //	if(!mmReturn) //重置设备成功，立即关闭设备
    ::waveInClose(m_hRecord); //关闭设备

    //Sleep(300); //等待一段时间，使buffer反馈完成
    printf("AudioDevMgr releaseRecorder waveInClose mmReturn=%d\r\n", mmReturn);
    recordMarkStop();
    return VL_SUCCESS;
}

vl_status AudioDevMgr::releaseRecorder(int handler)
{
    MMRESULT mmReturn = 0;
    if(handler != recordId)
    {
        printf("AudioDevMgr release recorder handler=%d, recordid=%d\r\n", handler, recordId);
        return VL_ERR_AUDDEV_ID;
    }
    putRecorder(handler);
    return VL_SUCCESS;
}

vl_bool AudioDevMgr::isRecording() const {

    return FALSE;//recording;

}

vl_status AudioDevMgr::recordDevInitial(const RecordParameter& param)
{
    int i;
    vl_status status = VL_SUCCESS;
    recordBufferSize = RECBUFFERSIZE;
    printf("AudioDevMgr initialRecorder recordBufferSize=%d\r\n", recordBufferSize);

    return VL_SUCCESS;
}

vl_status AudioDevMgr::recordDevRelease()
{
    vl_status ret = VL_SUCCESS;

    return ret;
}



