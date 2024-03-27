#include <cstdint>
#include <vector>
#include <cstring>
#include <cassert>

class BitstreamWriter {
 public:
    BitstreamWriter() {}
    ~BitstreamWriter() {
        delete[] m_buffer;
    }
    BitstreamWriter(const BitstreamWriter& other) = delete;
    
    BitstreamWriter& operator=(const BitstreamWriter& other) = delete;
    
    BitstreamWriter(BitstreamWriter&& other) {
        m_bufferSize = other.m_bufferSize;
        m_currentPos = other.m_currentPos;
        m_buffer = other.m_buffer;

        m_bufferSize = 0;
        m_currentPos = 0;
        other.m_buffer = nullptr;
    }
    
    BitstreamWriter& operator=(BitstreamWriter&& other) {
        if (this == &other) {
            return *this;
        }

        BitstreamWriter newThis(std::move(other));
        std::swap(newThis, *this);
    }

    template<typename... Args>
    void write(const Args&... values) {
        constexpr uint32_t dataSize = (sizeof(Args) + ... + 0);
        reallocateIfNeed(m_currentPos + dataSize);
        writeValuesPackInBuffer(&m_buffer[m_currentPos], values...);
        m_currentPos += dataSize;
    }

    void writeData(const char* data, uint32_t dataSize) {
        reallocateIfNeed(m_currentPos + dataSize);
        memcpy(&m_buffer[m_currentPos], data, dataSize);
        m_currentPos += dataSize;
    }

    char* data() {
        return m_buffer;
    }

    uint32_t size() {
        return m_currentPos;
    }

 private:
    template<typename T, typename... Args>
    void writeValuesPackInBuffer(char* position, const T& value, const Args&... values) {
        memcpy(position, &value, sizeof(T));
        writeValuesPackInBuffer(position + sizeof(T), values...);
    }

    template<typename T>
    void writeValuesPackInBuffer(char* position, const T& value) {
        memcpy(position, &value, sizeof(T));
    }

    void reallocateIfNeed(uint32_t minNeedSize) {
        if (minNeedSize > m_bufferSize) {
            uint32_t newSize = std::max(MIN_BUFFER_SIZE, m_bufferSize);
            while (newSize < minNeedSize) {
                newSize *= SCALE_FACTOR;
            }
            reallocate(newSize);
        }
    }

    void reallocate(uint32_t newSize) {
        char* newBuffer = new char[newSize];
        memcpy(newBuffer, m_buffer, m_currentPos);

        delete[] m_buffer;
        m_buffer = newBuffer;
        m_bufferSize = newSize;
    }

    static constexpr uint32_t MIN_BUFFER_SIZE = 1;
    static constexpr uint32_t SCALE_FACTOR = 2;
    char* m_buffer = nullptr;
    uint32_t m_bufferSize = 0;
    uint32_t m_currentPos = 0;
};

class BitstreamReader {
 public:
    BitstreamReader(char* data, uint32_t dataSize) : m_buffer(data), m_bufferSize(dataSize), m_currentPos(0) {}
    BitstreamReader(const BitstreamReader& other) = delete;
    BitstreamReader& operator=(const BitstreamReader& other) = delete;
    BitstreamReader(BitstreamReader&& other) = delete;
    BitstreamReader& operator=(BitstreamReader&& other) = delete;

    template<typename... Args>
    void read(Args&&... values) {
        constexpr uint32_t dataSize = (sizeof(Args) + ... + 0);
        assert(m_currentPos + dataSize <= m_bufferSize);

        readValuesPack(&m_buffer[m_currentPos], values...);
        m_currentPos += dataSize;
    }
    
    void readData(char* data, uint32_t dataSize) {
        assert(m_currentPos + dataSize <= m_bufferSize);
        memcpy(data, &m_buffer[m_currentPos], dataSize);
    }

    void skip(uint32_t bytesToSkip) {
        assert(m_currentPos + bytesToSkip <= m_bufferSize);
        m_currentPos += bytesToSkip;
    }

    template<typename T>
    class Skip {
     public:
        Skip() {};
        Skip(const T& value) {};
    };

 private:
    template<typename T, typename... Args>
    void readValuesPack(char* position, T& value, Args&&... values) {
        value = *reinterpret_cast<T*>(position);
        readValuesPack(position + sizeof(T), values...);
    }

    template<typename T, typename... Args>
    void readValuesPack(char* position, Skip<T> skip, Args&&... values) {
        readValuesPack(position + sizeof(T), values...);
    }

    template<typename T>
    void readValuesPack(char* position, Skip<T> skip) { }

    template<typename T>
    void readValuesPack(char* position, T& value) {
        value = *reinterpret_cast<T*>(position);
    }

    char* m_buffer;
    uint32_t m_bufferSize;
    uint32_t m_currentPos;
};