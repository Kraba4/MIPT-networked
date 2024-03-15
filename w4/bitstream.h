// #include <cstdint>
// #include <vector>
// #include <cstring>
// #include <cassert>

// class Bitstream {
//  public:
//     Bitstream() {}
//     Bitstream(char* data, uint32_t dataSize) {
//         m_buffer = new char[dataSize];
//         memcpy(m_buffer, data, dataSize);
//         m_bufferSize = dataSize;
//         m_currentPos = 0;
//     }
//     ~Bitstream() {
//         delete[] m_buffer;
//     }
//     Bitstream(const Bitstream& other) = delete;
//     Bitstream(Bitstream&& other) = delete;
//     Bitstream& operator=(const Bitstream& other) = delete;
//     Bitstream& operator=(Bitstream&& other) = delete;
//     // Bitstream(const Bitstream& other) {
//     //     m_bufferSize = other.m_bufferSize;
//     //     m_currentPos = other.m_currentPos;
//     //     m_buffer = new char[m_bufferSize];
//     //     memcpy(m_buffer, other.m_buffer, m_currentPos);
//     // }

//     // Bitstream(Bitstream&& other) {
//     //     m_bufferSize = other.m_bufferSize;
//     //     m_currentPos = other.m_currentPos;
//     //     m_buffer = other.m_buffer;

//     //     m_bufferSize = 0;
//     //     m_currentPos = 0;
//     //     other.m_buffer = nullptr;
//     // }
    
//     // Bitstream& operator=(const Bitstream& other) {
//     //     if (this == &other) {
//     //         return *this;
//     //     }

//     //     Bitstream newThis(other);
//     //     std::swap(newThis, *this);
//     // }

//     // Bitstream& operator=(Bitstream&& other) {
//     //     if (this == &other) {
//     //         return *this;
//     //     }

//     //     Bitstream newThis(std::move(other));
//     //     std::swap(newThis, *this);
//     // }

//     template<typename T>
//     void write(const T& value) {
//         if (m_currentPos + sizeof(value) > m_bufferSize) {
//             uint32_t newSize = std::max(MIN_BUFFER_SIZE, m_bufferSize);
//             while (newSize < m_currentPos + sizeof(value)) {
//                 newSize *= 2;
//             }
//             reallocate(newSize);
//         }

//         memcpy(&m_buffer[m_currentPos], &value, sizeof(value));
//         m_currentPos += sizeof(value);
//     }

//     template<typename T>
//     void read(T& value) {
//         value = *reinterpret_cast<T*>(&m_buffer[m_currentPos]);
//         m_currentPos += sizeof(value);
//     }

//     char* data() {
//         return m_buffer;
//     }

//     uint32_t size() {
//         return m_bufferSize;
//     }
    
//  private:

//     void reallocate(uint32_t newSize) {
//         char* newBuffer = new char[newSize];
//         memcpy(newBuffer, m_buffer, m_currentPos);
//         m_bufferSize = newSize;
//         delete[] m_buffer;
//         m_buffer = newBuffer;
//     }

//     static constexpr uint32_t MIN_BUFFER_SIZE = 16;
//     char* m_buffer = nullptr;
//     uint32_t m_bufferSize = 0;
//     uint32_t m_currentPos = 0;
// };