#include <cstdint>
#include <vector>
#include <cstring>
#include <cassert>
#include <iostream>
#include <bitset>

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

    void writePackedUint32(uint32_t value) {
        constexpr int n_chooseSizeBits = 2;
        // constexpr uint8_t uint8Indicator   = 0b00000000;
        constexpr uint16_t uint16Indicator = 0b01000000;
        constexpr uint32_t uint32Indicator = 0b10000000;
        assert(value < (1 << (32 - n_chooseSizeBits)));
        // std::cout << std::bitset<32>(value) << std::endl;
        if (value < (1 << (8 - n_chooseSizeBits))) {
            const uint8_t packedValue = static_cast<uint8_t>(value); // | uint8Indicator
            write(packedValue); 
        }
        else if (value < (1 << (16 - n_chooseSizeBits))) {
            const uint8_t packedValueFirstByte = static_cast<uint8_t>(value >> 8) | uint16Indicator;
            const uint8_t packedValueSecondByte = static_cast<uint8_t>(value & ((1 << 8) - 1));
            write(packedValueFirstByte, packedValueSecondByte);
        }
        else {
            // записываю и считываю по байту потому что байты могут по разному располагаться в памяти в разных системах
            // вот у меня задом наперед, может щас везде задом наперед располагаются, но я не уверен поэтому так делаю
            const uint8_t packedValueFirstByte = static_cast<uint8_t>(value >> 24) | uint32Indicator;
            const uint8_t packedValueSecondByte = static_cast<uint8_t>((value >> 16) & ((1 << 8) - 1));
            const uint8_t packedValueThirdByte = static_cast<uint8_t>((value >> 8) & ((1 << 8) - 1));
            const uint8_t packedValueFourthByte = static_cast<uint8_t>(value & ((1 << 8) - 1));
            write(packedValueFirstByte, packedValueSecondByte, packedValueThirdByte, packedValueFourthByte);
        } 
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
        readValuesPack(values...);
    }
    
    void readData(char* data, uint32_t dataSize) {
        assert(m_currentPos + dataSize <= m_bufferSize);
        memcpy(data, &m_buffer[m_currentPos], dataSize);
    }

    void readPackedUint32(uint32_t &value) {
        constexpr int n_chooseSizeBits = 2;
        constexpr uint8_t uint8Indicator = 0b00;
        constexpr uint8_t uint16Indicator = 0b01;
        constexpr uint8_t uint32Indicator = 0b10;
        uint8_t firstByte;
        read(firstByte);
        const uint8_t typeIndicator = firstByte >> (8 - n_chooseSizeBits);
        const uint8_t firstByteValue = firstByte & ((1 << (8 - n_chooseSizeBits)) - 1);
        if (typeIndicator == uint8Indicator) {
            value = static_cast<uint32_t>(firstByteValue);
        }
        else if (typeIndicator == uint16Indicator) {
            uint8_t secondByte;
            read(secondByte);
            value = (static_cast<uint32_t>(firstByteValue) << 8) |
                    (static_cast<uint32_t>(secondByte));
            // std::cout << std::bitset<32>(value) << std::endl;
        }
        else {
            uint8_t secondByte;
            uint8_t thirdByte;
            uint8_t fourthByte;
            read(secondByte);
            read(thirdByte);
            read(fourthByte);
            value = (static_cast<uint32_t>(firstByteValue) << 24) |
                    (static_cast<uint32_t>(secondByte) << 16) |
                    (static_cast<uint32_t>(thirdByte) << 8) |
                    (static_cast<uint32_t>(fourthByte));
        }
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
    void readValuesPack(T& value, Args&&... values) {
        assert(m_currentPos + sizeof(T) <= m_bufferSize);
        value = *reinterpret_cast<T*>(&m_buffer[m_currentPos]);
        m_currentPos += sizeof(T);
        readValuesPack(values...);
    }

    template<typename T, typename... Args>
    void readValuesPack(Skip<T> skip, Args&&... values) {
        assert(m_currentPos + sizeof(T) <= m_bufferSize);
        m_currentPos += sizeof(T);
        readValuesPack(values...);
    }

    template<typename T>
    void readValuesPack(Skip<T> skip) { 
        assert(m_currentPos + sizeof(T) <= m_bufferSize);
        m_currentPos += sizeof(T); 
    }

    template<typename T>
    void readValuesPack(T& value) {
        assert(m_currentPos + sizeof(T) <= m_bufferSize);
        value = *reinterpret_cast<T*>(&m_buffer[m_currentPos]);
        m_currentPos += sizeof(T);
    }

    char* m_buffer;
    uint32_t m_bufferSize;
    uint32_t m_currentPos;
};