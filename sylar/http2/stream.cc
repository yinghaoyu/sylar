#include "stream.h"
#include "http2_stream.h"
#include "sylar/log.h"

namespace sylar {
namespace http2 {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static const std::vector<std::string> s_state_strings = {
    "IDLE",
    "OPEN",
    "CLOSED",
    "RESERVED_LOCAL",
    "RESERVED_REMOTE",
    "HALF_CLOSE_LOCAL",
    "HALF_CLOSE_REMOTE",
};

std::string Stream::StateToString(State state) {
  uint8_t v = (uint8_t)state;
  if (v < 7) {
    return s_state_strings[7];
  }
  return "UNKNOW(" + std::to_string((uint32_t)v) + ")";
}

Stream::Stream(std::weak_ptr<Http2Stream> stm, uint32_t id)
    : m_stream(stm), m_state(State::IDLE), m_id(id) {}

std::shared_ptr<Http2Stream> Stream::getStream() const {
  return m_stream.lock();
}

int32_t Stream::handleRstStreamFrame(Frame::ptr frame, bool is_client) {
  m_state = State::CLOSED;
  return 0;
}

int32_t Stream::handleFrame(Frame::ptr frame, bool is_client) {
  int rt = 0;
  if (frame->header.type == (uint8_t)FrameType::HEADERS) {
    rt = handleHeadersFrame(frame, is_client);
  } else if (frame->header.type == (uint8_t)FrameType::DATA) {
    rt = handleDataFrame(frame, is_client);
  } else if (frame->header.type == (uint8_t)FrameType::RST_STREAM) {
    rt = handleRstStreamFrame(frame, is_client);
  }
  if (frame->header.flags & (uint8_t)FrameFlagHeaders::END_STREAM) {
    m_state = State::CLOSED;
    if (is_client) {
      if (!m_response) {
        m_response = std::make_shared<http::HttpResponse>(0x20);
      }
      if (m_recvHPack) {
        auto& m = m_recvHPack->getHeaders();
        for (auto& i : m) {
          m_response->setHeader(i.name, i.value);
        }
      }
      Http2InitResponseForRead(m_response);
    } else {
      if (!m_request) {
        m_request = std::make_shared<http::HttpRequest>(0x20);
      }
      if (m_recvHPack) {
        auto& m = m_recvHPack->getHeaders();
        for (auto& i : m) {
          m_request->setHeader(i.name, i.value);
        }
      }
      Http2InitRequestForRead(m_request);
    }
    SYLAR_LOG_DEBUG(g_logger) << "id=" << m_id << " is_client=" << is_client
                              << " req=" << m_request << " rsp=" << m_response;
  }
  return rt;
}

int32_t Stream::handleHeadersFrame(Frame::ptr frame, bool is_client) {
  auto headers = std::dynamic_pointer_cast<HeadersFrame>(frame->data);
  if (!headers) {
    SYLAR_LOG_ERROR(g_logger)
        << "Stream id=" << m_id << " handleHeadersFrame data not HeadersFrame "
        << frame->toString();
    return -1;
  }
  auto stream = getStream();
  if (!stream) {
    SYLAR_LOG_ERROR(g_logger)
        << "Stream id=" << m_id << " handleHeadersFrame stream is closed "
        << frame->toString();
    return -1;
  }

  if (!m_recvHPack) {
    m_recvHPack = std::make_shared<HPack>(stream->getRecvTable());
  }
  return m_recvHPack->parse(headers->data);
}

int32_t Stream::handleDataFrame(Frame::ptr frame, bool is_client) {
  auto data = std::dynamic_pointer_cast<DataFrame>(frame->data);
  if (!data) {
    SYLAR_LOG_ERROR(g_logger)
        << "Stream id=" << m_id << " handleDataFrame data not DataFrame "
        << frame->toString();
    return -1;
  }
  auto stream = getStream();
  if (!stream) {
    SYLAR_LOG_ERROR(g_logger)
        << "Stream id=" << m_id << " handleDataFrame stream is closed "
        << frame->toString();
    return -1;
  }
  if (is_client) {
    m_response = std::make_shared<http::HttpResponse>(0x20);
    m_response->setBody(data->data);
  } else {
    m_request = std::make_shared<http::HttpRequest>(0x20);
    m_request->setBody(data->data);
  }
  return 0;
}

int32_t Stream::sendResponse(http::HttpResponse::ptr rsp) {
  auto stream = getStream();
  if (!stream) {
    SYLAR_LOG_ERROR(g_logger)
        << "Stream id=" << m_id << " sendResponse stream is closed";
    return -1;
  }

  Http2InitResponseForWrite(rsp);

  Frame::ptr headers = std::make_shared<Frame>();
  headers->header.type = (uint8_t)FrameType::HEADERS;
  headers->header.flags = (uint8_t)FrameFlagHeaders::END_HEADERS;
  if (rsp->getBody().empty()) {
    headers->header.flags |= (uint8_t)FrameFlagHeaders::END_STREAM;
  }
  headers->header.identifier = m_id;
  HeadersFrame::ptr data;
  auto m = rsp->getHeaders();
  data = std::make_shared<HeadersFrame>();

  HPack hp(stream->getSendTable());
  std::vector<std::pair<std::string, std::string>> hs;
  for (auto& i : m) {
    hs.push_back(std::make_pair(sylar::ToLower(i.first), i.second));
  }
  hp.pack(hs, data->data);
  headers->data = data;
  int ok = stream->sendFrame(headers);
  if (ok < 0) {
    SYLAR_LOG_ERROR(g_logger)
        << "Stream id=" << m_id << " sendResponse send Headers fail";
    return ok;
  }
  if (!rsp->getBody().empty()) {
    Frame::ptr body = std::make_shared<Frame>();
    body->header.type = (uint8_t)FrameType::DATA;
    body->header.flags = (uint8_t)FrameFlagData::END_STREAM;
    body->header.identifier = m_id;
    auto data = std::make_shared<DataFrame>();
    data->data = rsp->getBody();
    body->data = data;
    ok = stream->sendFrame(body);
  }
  return ok;
}

int32_t Stream::sendFrame(Frame::ptr frame) {
  return 0;
}

Stream::ptr StreamManager::get(uint32_t id) {
  RWMutexType::ReadLock lock(m_mutex);
  auto it = m_streams.find(id);
  return it == m_streams.end() ? nullptr : it->second;
}

void StreamManager::add(Stream::ptr stream) {
  RWMutexType::WriteLock lock(m_mutex);
  m_streams[stream->getId()] = stream;
}

void StreamManager::del(uint32_t id) {
  RWMutexType::WriteLock lock(m_mutex);
  m_streams.erase(id);
}

}  // namespace http2
}  // namespace sylar
