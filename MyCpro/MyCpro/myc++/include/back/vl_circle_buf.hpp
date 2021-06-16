#ifndef __VL_CIRCLE_BUF_HPP__
#define __VL_CIRCLE_BUF_HPP__

#include "vl_types.h"
#include "vl_const.h"

/**
 * vl_media_buffer 只能存储固定采样率，固定单位采样位数（如8bits，16bits），固定声道数的声音样本
 * 一般来说，分配的缓冲区长度应该为帧大小的整数倍
 * 每次写入和读出，都应该为固定的长度，即一帧大小
 */

class vl_circle_buf
{
private:
  vl_int16     *buf;
  vl_size       capacity;
  /* 第一个可读偏移 */
  vl_offset     off_rd;
  /* 可读内容长度，以buf单位算 */
  vl_size       len;
public:
  vl_circle_buf(vl_size cap);
  virtual ~vl_circle_buf();

  /* 获取可写容量 */
  vl_size get_free_size();
  /* 获取有效数据长度 */
  vl_size get_length();
  /* 获取循环缓存容量 */
  vl_size get_capacity();
  /* 重置循环缓冲区，容量不变 */
  void reset();
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
  vl_bool is_empty();
  /**
   * 判断循环缓冲区是否已满
   */
  vl_bool is_full();
};

#endif
















