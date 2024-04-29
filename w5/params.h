constexpr uint32_t fixedDt = 20;
constexpr uint32_t FIXED_OFFSET = fixedDt * 3; // можно увеличить чтобы при большом rtt не было подергиваний из-за перенакатов
constexpr uint32_t SEND_TIMEOUT = 100;
constexpr uint32_t FPS = 60;
constexpr uint32_t TIME_PER_FRAME = (1.0 / static_cast<double>(FPS)) * 1000;