#ifndef __UNIT_CIRCLE_BUFFER_HPP__
#define __UNIT_CIRCLE_BUFFER_HPP__


#include "vl_types.h"
#include "vl_const.h"

/**
 * 一般来说，分配的缓冲区长度应该为帧大小的整数倍
 * 每次写入和读出，都应该为固定的长度，即一帧大小
 */


class UnitCircleBuffer
{
public:
  UnitCircleBuffer(vl_size cap = 0);
  virtual ~UnitCircleBuffer();

  /* 获取可写容量 */
  vl_size getFreeSize();
  /* 获取有效数据长度 */
  vl_size getLength();
  /* 获取循环缓存容量 */
  vl_size getCapacity();
  /* 重置循环缓冲区，容量不变 */
  void reset();
  void init(vl_size cap);
  /**
   * 获取缓冲区的指针，不直接拷贝数据，只是返回指针，但是数据会标志已读。
   * 注意：getRange操作无法进行回绕，缓冲区大小应该为每次读取的倍数。否则容易导致错乱。
   */
  vl_status getRange(vl_int16** dst, vl_size count);
  /*
   * 标记缓冲区域将写入数据。
   * 注意： markRange操作无法回绕，缓冲大小应该为标记大小的倍数，否则，容易导致错乱。
   */
  vl_int16* markRange(vl_size count);
  /** 
   * 从缓冲区读出数据
   * @param dst(I/O) : 读出的数据存放地址
   * @param count(I) : dst单位大小，read函数将尽量读满
   *  
   * @return 若缓冲区有数据可读，返回VL_SUCCESS；
   *         若缓冲区为空，返回VL_ERR_CCB_EMPTY_READ
   *         若读取异常（如缓冲区没有分配内存），返回VL_ERR_CCB_OP_FAILED
   */
  vl_status read(vl_int16 * dst, vl_size count);
  /**
   * 写内存到缓冲区
   * @param src(I)   : 将写入循环缓冲的内存
   * @param count(I) : 将写入循环缓冲的单位大小
   *
   * @return 若写入成功，返回VL_SUCCESS;
   *         若缓冲已满或剩余空间不足，返回VL_ERR_CCB_FULL_WRITE
   *         写入异常，返回VL_ERR_CCB_OP_FAILED
   */
  vl_status write(vl_int16 * src, vl_size count);
  /**
   * 判断循环缓冲区是否为空
   */
  vl_bool isEmpty();
  /**
   * 判断循环缓冲区是否已满
   */
  vl_bool isFull();
  /**
   * 流媒体播放可根据时候有数据播放选择填充缓冲
   */
  void closeWrite();
  vl_bool hasMoreData() const;
private:
  /* 缓冲 */
  vl_int16     *buf;
  /* 缓冲区大小 */
  vl_size       capacity;
  /* 第一个可读偏移 */
  vl_offset     off_rd;
  /* 可读内容长度，以buf单位算 */
  vl_size       len;
  /* 用于最后的数据不足期望大小时，若判断无数据输入，告知调用者 */
  vl_bool       moreData;
};

#endif
