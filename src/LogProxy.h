/** LogProxy -  .
 *
 * Copyright (C) 2018 Elektronik Workshop <hoi@elektronikworkshop.ch>
 * http://elektronikworkshop.ch
 *
 *
 ***
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _EW_LOG_PROXY_H_
#define _EW_LOG_PROXY_H_

#if defined(WIRING) && WIRING >= 100
  #include <Wiring.h>
#elif defined(ARDUINO) && ARDUINO >= 100
  #include <Arduino.h>
#else
  #include <WProgram.h>
#endif

#ifdef ARDUINO_ARCH_AVR
# include <limits.h>  /* INT_MIN, ... */
  /* argh, no std::forward, what a shame! */
#else
# include <climits>
# include <utility>  /* std::forward */
# define HAS_STD_FORWARD 1
# define HAS_ULL 1
#endif

template<uint8_t _MaxStreams>
class LogProxy
  : public Print
{
public:
  static const unsigned int MaxStreams = _MaxStreams;
  LogProxy(bool enabled = true)
    : m_streams{0}
    , m_n(0)
    , m_enabled(enabled)
  { }
  bool addClient(Print& stream)
  {
    for (uint8_t i = 0; i < MaxStreams; i++) {
      if (m_streams[i] == &stream) {
        return true;
      }
    }
    for (uint8_t i = 0; i < MaxStreams; i++) {
      if (not m_streams[i]) {
        m_streams[i] = &stream;
        m_n++;
        return true;
      }
    }
    return false;
  }
  bool removeClient(Print& stream)
  {
    for (uint8_t i = 0; i < MaxStreams; i++) {
      if (m_streams[i] == &stream) {
        m_streams[i] = NULL;
        m_n--;
        return true;
      }
    }
    return false;
  }
  void enable(bool enable)
  {
    m_enabled = enable;
  }
  bool isEnabled() const
  {
    return m_enabled;
  }
protected:
  virtual size_t write(uint8_t c)
  {
    if (not m_enabled) {
      return 1;
    }
    
    size_t ret = Serial.write(c);
    
    if (m_n) {
      for (uint8_t i = 0; i < MaxStreams; i++) {
        if (m_streams[i]) {
          m_streams[i]->write(c);
        }
      }
    }
    return ret;
  }
private:
  Print* m_streams[MaxStreams];
  uint8_t m_n;
  bool m_enabled;
};

template<unsigned int _BufferSize, unsigned int _MaxStreams>
class BufferedLogProxy
  : public LogProxy<_MaxStreams>
{
public:
  static const unsigned int BufferSize = _BufferSize;
  
  BufferedLogProxy(bool enabled = true)
    : LogProxy<_MaxStreams>(enabled)
    , m_buffer{0}
    , m_writeIndex(0)
  { }
  struct BufNfo
  {
    const char* a;
    unsigned int na;
    const char* b;
    unsigned int nb;
  };
  void getBuffer(BufNfo& nfo) const
  {
    nfo.a = m_buffer + m_writeIndex;
    nfo.na = BufferSize - m_writeIndex;
    nfo.b = m_buffer;
    nfo.nb = m_writeIndex;
  }

  /** Experimental */
  static void skipLines(unsigned int& skip, unsigned int& i, const char* buf, unsigned int count)
  {
    for (; i < count and skip;) {
      char c = *(buf + i);
      if (c == '\n') {
        skip--;
      }
      if (not c) {
        return;
      }
      i++;
    }
  }
  static void prtLines(Print& prt, unsigned int& numLines, const char* buf, unsigned int count)
  {
    for (unsigned int i = 0; i < count and numLines; i++) {
      char c = *(buf + i);
      if (not c) {
        return;
      }
      prt << c;
      if (c == '\n') {
        --numLines;
        if (numLines == 0) {
          return;
        }
      }
    }
  }
  bool
  print(Print& prt, int start, int end)
  {
    unsigned int numLinesRequested = 10, skip = 0, n;

    /* only count given */
    if (start and not end) {
      if (start > 0) {
        numLinesRequested = start;
      } else {
        numLinesRequested = UINT_MAX;
      }
    } else if (start and end) {
      if (start < 0) {
        prt << "<start> can not be negative when requesting a range\n";
        return false;
      }
      if (end < 0) {
        numLinesRequested = UINT_MAX;
      } else {
        if (end <= start) {
          prt << "<start> must be smaller than <end>\n";
          return false;
        }
        numLinesRequested = end - start + 1;
      }
      skip = start;
    }
    
    BufNfo nfo;
    getBuffer(nfo);

    n = numLinesRequested;

    prt << "----\n";
    
    unsigned int i = 0;
    skipLines(skip, i, nfo.a, nfo.na);
    if (not skip) {
      prtLines(prt, n, nfo.a + i, nfo.na - i);
    }
    i = 0;
    if (skip) {
      skipLines(skip, i, nfo.b, nfo.nb);
    }
    if (not skip) {
      prtLines(prt, n, nfo.b + i, nfo.nb - i);
    }
    if (numLinesRequested - n == 0) {
      prt << "history clean\n";
    } else {
      prt
        << "----\n"
        << numLinesRequested - n << " lines\n"
        ;
    }
    return true;
  }

protected:

  virtual size_t write(uint8_t c)
  {
    if (not this->isEnabled()) {
      return 1;
    }

    /* write to buffer */
    
    m_buffer[m_writeIndex++] = c;
    if (m_writeIndex == BufferSize) {
      m_writeIndex = 0;
    }

    return LogProxy<_MaxStreams>::write(c);
  }
private:
  char m_buffer[BufferSize];
  unsigned int m_writeIndex;
};

#endif /* _EW_LOG_PROXY_H_ */

