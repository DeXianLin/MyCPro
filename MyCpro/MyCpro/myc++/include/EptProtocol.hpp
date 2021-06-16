#ifndef __EPT_PROTOCOL_HPP__
#define __EPT_PROTOCOL_HPP__

#include "vl_types.h"
#include "vl_const.h"
#include "EptData.hpp"



/**
 * 网络协议编码解码基类。
 * 具体的协议编解码对应一个编解码枚举号。
 * 大部分网络编码解码只是在头部和尾部简单加上包头和包尾，这种类型的编解码可以优化内存拷贝次数，通过isSimpleWrap()接口判断。
 * 编码操作不申请内存；
 * 解码时先调用parse函数，确认包是否可使用该解码实例解码，若能解码，由外部申请一个对象传入解码。
 * 具体的内存策略需要看具体的编解码实现。
 */

class EptProtocol {
public:
    EptProtocol(EProtocol protoId, vl_bool isSimpleWarp = VL_TRUE) : eProtocol(protoId), simpleWrap(isSimpleWarp)
      {};


    virtual ~EptProtocol(){};

    /**
   * 获取编解码枚举号
   */
    EProtocol getProtocol() const { return eProtocol; }

    /*
   * 编码
   * obj : [in] 结构化数据，根据结构化数据最终可编码为平坦数据
   * packet : [in|out] 编码结束数据，内存由外部申请。优先使用packet地址，若packet地址为空，使用
   *          obj的packet地址，若packet地址为空，失败。
   * pktlen : [in|out] 缓冲区大小|有效数据大小
   * @return : 成功返回VL_SUCCESS
   */
    virtual vl_status encode(ProtoObject* obj, void* packet = NULL, vl_size* pktlen = NULL) = 0;

    /*
   * 解码
   * struData : [out] 解码数据，解码数据内存由decode产生
   * platData : [in] 接受的数据
   * len : platData长度
   *
   * 注意：调用流程，一定要先parse为一个合法的协议数据流，才能调用decode进行解码，否则
   * 可能会发生未知错误。
   * @return : 成功返回VL_SUCCESS
   */
    virtual vl_status decode(ProtoObject* obj, void* packet, vl_size pktlen) = 0;

    /**
   * 判断某二进制数据是否能解码。
   */
    virtual vl_bool parse(void *packet, vl_size pktlen) = 0;
    virtual ProtoObject* allocObject(void* packet, vl_size pktlen) = 0;
    virtual void freeObject(ProtoObject*) = 0;

    /**
   * 代表该协议的编码是否为包头+内容+包尾，大部分网络编码都属于这种类型。
   * 属于简单类型的编码，则可以优化减少内存拷贝次数。
   * 调用编码时，编码器只是简单的在内容的前面或后面加上包头包围，包内容本身不做拷贝。
   * 调用解码时，解码器将内容的地址和大小等内容解析，内容不做拷贝。
   */
    virtual vl_bool isSimpleWrap() const {return simpleWrap;};

    /**
   * 获取编码头部和尾部所需额外空间大小，又具体的协议编辑码实例确定，部分需要有参数实例化后才能确定。
   */
    static vl_size getPrefixSize(void* param) { return 0; };
    static vl_size getPostfixSize(void* param) { return 0; };

    /**
   * 对于非simpleWrap类型的编解码，通过该接口获取编码所需的缓冲大小，不可以返回0。
   */
    static vl_size getMaxPacketSize() { return 0; };
private:
    EProtocol eProtocol;
    vl_bool simpleWrap;
};

#endif

