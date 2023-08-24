#include "bytearray.h"
#include <string.h>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "endian.h"
#include "log.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

ByteArray::Node::Node(size_t s) : ptr(new char[s]), next(nullptr), size(s) {}

ByteArray::Node::Node() : ptr(nullptr), next(nullptr), size(0) {}

ByteArray::Node::~Node() {}

void ByteArray::Node::free() {
  if (ptr) {
    delete[] ptr;
    ptr = nullptr;
  }
}

ByteArray::ByteArray(size_t base_size)
    : m_baseSize(base_size),
      m_position(0),
      m_capacity(base_size),
      m_size(0),
      m_endian(SYLAR_BIG_ENDIAN),
      m_owner(true),
      m_root(new Node(base_size)),
      m_cur(m_root) {}

ByteArray::ByteArray(void* data, size_t size, bool owner)
    : m_baseSize(size),
      m_position(0),
      m_capacity(size),
      m_size(size),
      m_endian(SYLAR_BIG_ENDIAN),
      m_owner(owner) {
  m_root = new Node();
  m_root->ptr = (char*)data;
  m_root->size = size;
  m_cur = m_root;
}

ByteArray::~ByteArray() {
  Node* tmp = m_root;
  while (tmp) {
    m_cur = tmp;
    tmp = tmp->next;
    if (m_owner) {
      m_cur->free();
    }
    delete m_cur;
  }
}

bool ByteArray::isLittleEndian() const {
  return m_endian == SYLAR_LITTLE_ENDIAN;
}

void ByteArray::setIsLittleEndian(bool val) {
  if (val) {
    m_endian = SYLAR_LITTLE_ENDIAN;
  } else {
    m_endian = SYLAR_BIG_ENDIAN;
  }
}

void ByteArray::writeFint8(int8_t value) {
  write(&value, sizeof(value));
}

void ByteArray::writeFuint8(uint8_t value) {
  write(&value, sizeof(value));
}
void ByteArray::writeFint16(int16_t value) {
  if (m_endian != SYLAR_BYTE_ORDER) {
    value = byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFuint16(uint16_t value) {
  if (m_endian != SYLAR_BYTE_ORDER) {
    value = byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFint32(int32_t value) {
  if (m_endian != SYLAR_BYTE_ORDER) {
    value = byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFuint32(uint32_t value) {
  if (m_endian != SYLAR_BYTE_ORDER) {
    value = byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFint64(int64_t value) {
  if (m_endian != SYLAR_BYTE_ORDER) {
    value = byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFuint64(uint64_t value) {
  if (m_endian != SYLAR_BYTE_ORDER) {
    value = byteswap(value);
  }
  write(&value, sizeof(value));
}

// 根据移码规则，把负数转化为正数，保证 bit 为 1 都是有效值，而不是符号
// -2 -> 3
// -1 -> 1
//  0 -> 0
//  1 -> 2
//  2 -> 4
// 可以看到，负数是奇数，正数是偶数，任意值都是唯一且连续
static uint32_t EncodeZigzag32(const int32_t& v) {
  if (v < 0) {
    return ((uint32_t)(-v)) * 2 - 1;
  } else {
    return v * 2;
  }
}

static uint64_t EncodeZigzag64(const int64_t& v) {
  if (v < 0) {
    return ((uint64_t)(-v)) * 2 - 1;
  } else {
    return v * 2;
  }
}

static int32_t DecodeZigzag32(const uint32_t& v) {
  return (v >> 1) ^ -(v & 1);
}

static int64_t DecodeZigzag64(const uint64_t& v) {
  return (v >> 1) ^ -(v & 1);
}

void ByteArray::writeInt32(int32_t value) {
  // 先通过 Zigzag 将负数转成正数，再压缩
  writeUint32(EncodeZigzag32(value));
}

// 每个字节浪费 1 bit 表示高位是否存在有效数据
// 实际上，小数字用到的比较多，比如正常情况下，数字 6 用 int 需要 4 个字节，
// 而在这种方法下，可以将 6 的表示压缩为 1 个字节
void ByteArray::writeUint32(uint32_t value) {
  uint8_t tmp[5];
  uint8_t i = 0;
  while (value >= 0x80) {
    tmp[i++] = (value & 0x7F) | 0x80;
    value >>= 7;
  }
  tmp[i++] = value;
  write(tmp, i);
}

void ByteArray::writeInt64(int64_t value) {
  writeUint64(EncodeZigzag64(value));
}

void ByteArray::writeUint64(uint64_t value) {
  uint8_t tmp[10];
  uint8_t i = 0;
  while (value >= 0x80) {
    tmp[i++] = (value & 0x7F) | 0x80;
    value >>= 7;
  }
  tmp[i++] = value;
  write(tmp, i);
}

void ByteArray::writeFloat(float value) {
  uint32_t v;
  // memcpy 可将 float double 无损转存为 uint32_t
  memcpy(&v, &value, sizeof(value));
  writeFuint32(v);
}

void ByteArray::writeDouble(double value) {
  uint64_t v;
  memcpy(&v, &value, sizeof(value));
  writeFuint64(v);
}

void ByteArray::writeStringF16(const std::string& value) {
  writeFuint16(value.size());
  write(value.c_str(), value.size());
}

void ByteArray::writeStringF32(const std::string& value) {
  writeFuint32(value.size());
  write(value.c_str(), value.size());
}

void ByteArray::writeStringF64(const std::string& value) {
  writeFuint64(value.size());
  write(value.c_str(), value.size());
}

void ByteArray::writeStringVint(const std::string& value) {
  writeUint64(value.size());
  write(value.c_str(), value.size());
}

void ByteArray::writeStringWithoutLength(const std::string& value) {
  write(value.c_str(), value.size());
}

int8_t ByteArray::readFint8() {
  int8_t v;
  read(&v, sizeof(v));
  return v;
}

uint8_t ByteArray::readFuint8() {
  uint8_t v;
  read(&v, sizeof(v));
  return v;
}

#define XX(type)                      \
  type v;                             \
  read(&v, sizeof(v));                \
  if (m_endian == SYLAR_BYTE_ORDER) { \
    return v;                         \
  } else {                            \
    return byteswap(v);               \
  }

int16_t ByteArray::readFint16() {
  XX(int16_t);
}
uint16_t ByteArray::readFuint16() {
  XX(uint16_t);
}

int32_t ByteArray::readFint32() {
  XX(int32_t);
}

uint32_t ByteArray::readFuint32() {
  XX(uint32_t);
}

int64_t ByteArray::readFint64() {
  XX(int64_t);
}

uint64_t ByteArray::readFuint64() {
  XX(uint64_t);
}

#undef XX

int32_t ByteArray::readInt32() {
  return DecodeZigzag32(readUint32());
}

uint32_t ByteArray::readUint32() {
  uint32_t result = 0;
  // 每个字节 7 个有效位
  for (int i = 0; i < 32; i += 7) {
    uint8_t b = readFuint8();
    if (b < 0x80) {
      result |= ((uint32_t)b) << i;
      break;
    } else {
      result |= (((uint32_t)(b & 0x7f)) << i);
    }
  }
  return result;
}

int64_t ByteArray::readInt64() {
  return DecodeZigzag64(readUint64());
}

uint64_t ByteArray::readUint64() {
  uint64_t result = 0;
  for (int i = 0; i < 64; i += 7) {
    uint8_t b = readFuint8();
    if (b < 0x80) {
      result |= ((uint64_t)b) << i;
      break;
    } else {
      result |= (((uint64_t)(b & 0x7f)) << i);
    }
  }
  return result;
}

float ByteArray::readFloat() {
  uint32_t v = readFuint32();
  float value;
  memcpy(&value, &v, sizeof(v));
  return value;
}

double ByteArray::readDouble() {
  uint64_t v = readFuint64();
  double value;
  memcpy(&value, &v, sizeof(v));
  return value;
}

std::string ByteArray::readStringF16() {
  uint16_t len = readFuint16();
  std::string buff;
  buff.resize(len);
  read(&buff[0], len);
  return buff;
}

std::string ByteArray::readStringF32() {
  uint32_t len = readFuint32();
  std::string buff;
  buff.resize(len);
  read(&buff[0], len);
  return buff;
}

std::string ByteArray::readStringF64() {
  uint64_t len = readFuint64();
  std::string buff;
  buff.resize(len);
  read(&buff[0], len);
  return buff;
}

std::string ByteArray::readStringVint() {
  uint64_t len = readUint64();
  std::string buff;
  buff.resize(len);
  read(&buff[0], len);
  return buff;
}

void ByteArray::clear() {
  m_position = m_size = 0;
  m_capacity = m_baseSize;
  Node* tmp = m_root->next;
  while (tmp) {
    m_cur = tmp;
    tmp = tmp->next;
    if (m_owner) {
      m_cur->free();
    }
    delete m_cur;
  }
  m_cur = m_root;
  m_root->next = NULL;
}

void ByteArray::write(const void* buf, size_t size) {
  if (size == 0) {
    return;
  }
  // 尝试扩容
  addCapacity(size);

  size_t npos = m_position % m_baseSize;  // 节点内偏移
  size_t ncap = m_cur->size - npos;       // 当前节点的剩余大小
  size_t bpos = 0;                        // 在 buf 内的偏移

  while (size > 0) {
    if (ncap >= size) {
      // 写长度 <= 节点剩余大小
      memcpy(m_cur->ptr + npos, (const char*)buf + bpos, size);
      if (m_cur->size == (npos + size)) {
        // 当前节点写完了，切换到下一块
        m_cur = m_cur->next;
      }
      m_position += size;
      bpos += size;
      size = 0;
    } else {
      // 写长度 > 节点剩余大小
      memcpy(m_cur->ptr + npos, (const char*)buf + bpos, ncap);
      m_position += ncap;
      bpos += ncap;
      size -= ncap;
      m_cur = m_cur->next;
      ncap = m_cur->size;
      npos = 0;
    }
  }

  if (m_position > m_size) {
    // 扩容过，更新一下 array 大小
    m_size = m_position;
  }
}

void ByteArray::read(void* buf, size_t size) {
  if (size > getReadSize()) {
    throw std::out_of_range("not enough len");
  }

  size_t npos = m_position % m_baseSize;  // 在节点内的偏移
  size_t ncap = m_cur->size - npos;       // 节点剩余大小
  size_t bpos = 0;                        // 在 buf 的偏移
  while (size > 0) {
    if (ncap >= size) {
      // 读取长度 <= 节点剩余大小
      memcpy((char*)buf + bpos, m_cur->ptr + npos, size);
      if (m_cur->size == (npos + size)) {
        // 当前节点读完了，切换到下一块
        m_cur = m_cur->next;
      }
      m_position += size;
      bpos += size;
      size = 0;
    } else {
      // 读取长度 > 节点剩余大小
      memcpy((char*)buf + bpos, m_cur->ptr + npos, ncap);
      m_position += ncap;
      bpos += ncap;
      size -= ncap;
      m_cur = m_cur->next;
      ncap = m_cur->size;
      npos = 0;
    }
  }
}

void ByteArray::read(void* buf, size_t size, size_t position) const {
  if (size > getReadSize()) {
    throw std::out_of_range("not enough len");
  }

  size_t npos = position % m_baseSize;
  size_t ncap = m_cur->size - npos;
  size_t bpos = 0;
  Node* cur = m_cur;
  while (size > 0) {
    if (ncap >= size) {
      memcpy((char*)buf + bpos, cur->ptr + npos, size);
      if (cur->size == (npos + size)) {
        cur = cur->next;
      }
      position += size;
      bpos += size;
      size = 0;
    } else {
      memcpy((char*)buf + bpos, cur->ptr + npos, ncap);
      position += ncap;
      bpos += ncap;
      size -= ncap;
      cur = cur->next;
      ncap = cur->size;
      npos = 0;
    }
  }
}

void ByteArray::setPosition(size_t v) {
  if (v > m_capacity) {
    throw std::out_of_range("set_position out of range");
  }
  m_position = v;
  if (m_position > m_size) {
    // 如果 arr 实际保存的 size < position
    // 我们也强制调整 size 为 position，即使 [size, position] 这段区间内数据为 0
    m_size = m_position;
  }
  // cur 移动到 position 位置对应的节点
  m_cur = m_root;
  while (v > m_cur->size) {
    v -= m_cur->size;
    m_cur = m_cur->next;
  }
  if (v == m_cur->size) {
    m_cur = m_cur->next;
  }
}

bool ByteArray::writeToFile(const std::string& name, bool with_md5) const {
  std::ofstream ofs;
  ofs.open(name, std::ios::trunc | std::ios::binary);
  if (!ofs) {
    SYLAR_LOG_ERROR(g_logger)
        << "writeToFile name=" << name << " error , errno=" << errno
        << " errstr=" << strerror(errno);
    return false;
  }
  // 读取 [position, size] 区间内的数据到文件
  int64_t read_size = getReadSize();
  int64_t pos = m_position;
  Node* cur = m_cur;

  while (read_size > 0) {
    int diff = pos % m_baseSize;
    int64_t len =
        (read_size > (int64_t)m_baseSize ? m_baseSize : read_size) - diff;
    ofs.write(cur->ptr + diff, len);
    cur = cur->next;
    pos += len;
    read_size -= len;
  }

  if (with_md5) {
    std::ofstream ofs_md5(name + ".md5");
    ofs_md5 << getMd5();
  }

  return true;
}

bool ByteArray::readFromFile(const std::string& name) {
  std::ifstream ifs;
  ifs.open(name, std::ios::binary);
  if (!ifs) {
    SYLAR_LOG_ERROR(g_logger)
        << "readFromFile name=" << name << " error, errno=" << errno
        << " errstr=" << strerror(errno);
    return false;
  }

  std::shared_ptr<char> buff(new char[m_baseSize],
                             [](char* ptr) { delete[] ptr; });
  while (!ifs.eof()) {
    ifs.read(buff.get(), m_baseSize);
    write(buff.get(), ifs.gcount());
  }
  return true;
}

void ByteArray::addCapacity(size_t size) {
  if (size == 0) {
    return;
  }
  size_t old_cap = getCapacity();
  if (old_cap >= size) {
    return;
  }
  // 需要扩容
  size = size - old_cap;
  // 扩容需要的节点数
  size_t count = ceil(1.0 * size / m_baseSize);
  Node* tmp = m_root;
  while (tmp->next) {
    tmp = tmp->next;
  }
  // 把扩容的节点加入链表
  Node* first = NULL;
  for (size_t i = 0; i < count; ++i) {
    tmp->next = new Node(m_baseSize);
    if (first == NULL) {
      first = tmp->next;
    }
    tmp = tmp->next;
    m_capacity += m_baseSize;
  }

  if (old_cap == 0) {
    m_cur = first;
  }
}

std::string ByteArray::toString() const {
  std::string str;
  str.resize(getReadSize());
  if (str.empty()) {
    return str;
  }
  read(&str[0], str.size(), m_position);
  return str;
}

std::string ByteArray::toHexString() const {
  std::string str = toString();
  std::stringstream ss;

  for (size_t i = 0; i < str.size(); ++i) {
    if (i > 0 && i % 32 == 0) {
      ss << std::endl;
    }
    ss << std::setw(2) << std::setfill('0') << std::hex << (int)(uint8_t)str[i]
       << " ";
  }

  return ss.str();
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers,
                                   uint64_t len) const {
  // 获取待读取的长度，最大长度为 size - position
  len = len > getReadSize() ? getReadSize() : len;
  if (len == 0) {
    return 0;
  }

  uint64_t size = len;
  // npos 为节点内偏移
  size_t npos = m_position % m_baseSize;
  // ncap 为剩余需要读取的大小
  size_t ncap = m_cur->size - npos;
  struct iovec iov;
  Node* cur = m_cur;

  while (len > 0) {
    if (ncap >= len) {
      // 读取节点内的数据
      iov.iov_base = cur->ptr + npos;
      iov.iov_len = len;
      len = 0;
    } else {
      // 读取的不止一个节点
      iov.iov_base = cur->ptr + npos;
      iov.iov_len = ncap;
      len -= ncap;
      cur = cur->next;
      ncap = cur->size;
      npos = 0;
    }
    buffers.push_back(iov);
  }
  return size;
}

// 从 position 处开始读取，原理与上面函数相同
uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len,
                                   uint64_t position) const {
  len = len > getReadSize() ? getReadSize() : len;
  if (len == 0) {
    return 0;
  }

  uint64_t size = len;

  size_t npos = position % m_baseSize;
  size_t count = position / m_baseSize;
  Node* cur = m_root;
  while (count > 0) {
    cur = cur->next;
    --count;
  }

  size_t ncap = cur->size - npos;
  struct iovec iov;
  while (len > 0) {
    if (ncap >= len) {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len = len;
      len = 0;
    } else {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len = ncap;
      len -= ncap;
      cur = cur->next;
      ncap = cur->size;
      npos = 0;
    }
    buffers.push_back(iov);
  }
  return size;
}

uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
  if (len == 0) {
    return 0;
  }
  addCapacity(len);
  uint64_t size = len;

  size_t npos = m_position % m_baseSize;
  size_t ncap = m_cur->size - npos;
  struct iovec iov;
  Node* cur = m_cur;
  while (len > 0) {
    if (ncap >= len) {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len = len;
      len = 0;
    } else {
      iov.iov_base = cur->ptr + npos;
      iov.iov_len = ncap;

      len -= ncap;
      cur = cur->next;
      ncap = cur->size;
      npos = 0;
    }
    buffers.push_back(iov);
  }
  return size;
}

std::string ByteArray::getMd5() const {
  std::vector<iovec> buffers;
  getReadBuffers(buffers, -1, 0);
  return sylar::md5sum(buffers);
}

}  // namespace sylar
