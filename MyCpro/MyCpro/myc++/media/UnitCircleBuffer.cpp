
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "UnitCircleBuffer.hpp"

#define _MALLOC  malloc
#define _FREE    free
#define _MEMSET   memset

UnitCircleBuffer::UnitCircleBuffer(vl_size cap) {
    buf = NULL;
    off_rd = 0;
    len = 0;
    moreData = VL_TRUE;
    capacity = 0;

    if(cap != 0) {
        init(cap);
    }
}

void UnitCircleBuffer::init(vl_size cap) {
    if(buf != NULL) {
        delete[] buf;
        buf = NULL;
        capacity = 0;
    }

    capacity = cap;
    buf = new vl_int16[capacity];
    assert(this->buf);
    memset((void *)buf, 0, sizeof(vl_uint16) * capacity);
}

UnitCircleBuffer::~UnitCircleBuffer () {
    delete[] buf;
}

vl_size UnitCircleBuffer::getFreeSize() {
    return capacity - len;
}

vl_size UnitCircleBuffer::getCapacity() {
    return capacity;
}

vl_size UnitCircleBuffer::getLength() {
    return len;
}

vl_status UnitCircleBuffer::write(vl_int16 *src, vl_size count) {
    /* 缓冲区剩余容量不足 */
    if(count > (capacity - len)) {
        printf("ccb : write operation failed, left = %d, count = %d", capacity - len, count);
        return VL_ERR_CCB_FULL_WRITE;
    }

    /* 查找第一个可写偏移 */
    vl_offset off_wt = (off_rd + len) % capacity;
    vl_int16 * w_ptr = buf + off_wt;

    if(off_wt < off_rd) { /* 若可写偏移在可读偏移前，直接拷贝 */
        assert((off_rd - off_wt) >= count);
        memcpy((void*)w_ptr, (const void *)src, sizeof(vl_int16) * count);
    } else { /* 可写偏移在可读偏移后面 */
        vl_size tail_left = capacity - off_wt;

        if(tail_left >= count) { /* 缓冲区尾部容量足够写入 */
            memcpy((void*)w_ptr, (const void *) src, sizeof(vl_int16) * count);
        } else { /* 缓冲区尾部空间不足，分两段存放 */
            vl_size append_size = count - tail_left;
            memcpy((void *)w_ptr, (const void *)src, sizeof(vl_int16) * tail_left);
            memcpy((void *)buf, (const void *)(src + tail_left), sizeof(vl_int16) * append_size);
        }
    }

    /* 添加可读缓冲区大小 */
    len += count;
    return VL_SUCCESS;
}

vl_status UnitCircleBuffer::getRange(vl_int16** dst, vl_size count) {
    /* 剩余内容不足或产生回绕 */
    if(len < count) {
        //	LOGW("cbb len=%d, not enough buffer", len);
        return VL_ERR_CCB_EMPTY_READ;
    }

    if(capacity - off_rd < count) {
        //	LOGW("cbb get range error, can't run hear, query=%d", count);
        return VL_ERR_CCB_EMPTY_READ;
    }

    /* 将地址赋值给参数 */
    *dst = buf + off_rd;

    len -= count;
    off_rd = (off_rd + count) % capacity;
    return VL_SUCCESS;
}

vl_int16* UnitCircleBuffer::markRange(vl_size count) {
    /* 写缓冲不足 */
    if(count > (capacity - len)) {
        printf("ccb : mark operation failed, left = %d, count = %d", capacity - len, count);
        return NULL;
    }

    /* 判断产生回绕情况 */
    vl_size wdOff = (off_rd + len) % capacity;
    vl_size rightLeft;

    //  || (wdOff > off_rd && (count > (capacity - wdOff)))

    if(wdOff >= off_rd && (count > (capacity - wdOff))) {
        printf("ccb : mark operation failed, left = %d, count = %d", capacity - len, count);
        return NULL;
    }

    vl_int16* wdPtr = buf + wdOff;
    len += count;
    return wdPtr;
}

vl_status UnitCircleBuffer::read(vl_int16 *dst, vl_size count) {
    if(count > len) {
        //	LOGW("ccb : readable buffer size=%d is less than count=%d", len, count);
        return VL_ERR_CCB_EMPTY_READ;
    }

    vl_int16 * r_ptr = buf + off_rd;

    /* 可读缓冲区足够 */
    if(((off_rd + count) / capacity) >= 1){ /* 尾部可读缓冲区不足，同时也说明缓冲区尾部已满 */
        vl_size tail_left = capacity - off_rd;
        memcpy((void *) dst, (const void *) r_ptr, sizeof(vl_uint16) * tail_left);
        memcpy((void *) (dst + tail_left) , (const void *) buf , sizeof(vl_uint16) * (count - tail_left));
    } else {
        memcpy((void *) dst, (const void *) r_ptr, sizeof(vl_uint16) * count);
    }

    /* 减小可读缓冲区大小 */
    len -= count;
    off_rd = (off_rd + count) % capacity;

    //  LOGI("ccb : read, off_rd=%d, length=%d, readCount=%d",off_rd, len, count);
    return VL_SUCCESS;
}

void UnitCircleBuffer::reset() {
    memset((void*)buf, 0, sizeof(vl_uint16) * capacity);
    off_rd = 0;
    len = 0;
}

vl_bool UnitCircleBuffer::isFull() {
    if(len < capacity) {
        return VL_FALSE;
    }
    return VL_TRUE;
}

vl_bool UnitCircleBuffer::isEmpty() {
    if(len == 0) {
        return VL_TRUE;
    }
    return VL_FALSE;
}

void UnitCircleBuffer::closeWrite() {
    printf("UnitCircleBuffer write op closed ...");
    moreData = VL_FALSE;
}

vl_bool UnitCircleBuffer::hasMoreData() const {
    return moreData;
}
