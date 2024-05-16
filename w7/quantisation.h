#pragma once
#include "mathUtils.h"
#include <limits>
#include <tuple>

template<typename T>
T pack_float(float v, float lo, float hi, int num_bits)
{
  T range = (1 << num_bits) - 1;//std::numeric_limits<T>::max();
  return range * ((clamp(v, lo, hi) - lo) / (hi - lo));
}

template<typename T>
float unpack_float(T c, float lo, float hi, int num_bits)
{
  T range = (1 << num_bits) - 1;//std::numeric_limits<T>::max();
  return float(c) / range * (hi - lo) + lo;
}

template<typename T, int num_bits>
struct PackedFloat
{
  T packedVal;

  PackedFloat(float v, float lo, float hi) { pack(v, lo, hi); }
  PackedFloat(T compressed_val) : packedVal(compressed_val) {}

  void pack(float v, float lo, float hi) { packedVal = pack_float<T>(v, lo, hi, num_bits); }
  float unpack(float lo, float hi) { return unpack_float<T>(packedVal, lo, hi, num_bits); }
};

typedef PackedFloat<uint8_t, 4> float4bitsQuantized;

struct ValuesRange {
  float lo;
  float hi;
};

template<typename T, int num_bits1, int num_bits2>
struct PackedVec2
{
  T packedVal;

  PackedVec2(float v1, const ValuesRange& r1, 
              float v2, const ValuesRange& r2) { pack(v1, r1, v2, r2); }
  PackedVec2(T compressed_val) : packedVal(compressed_val) {}

  void pack(float v1, const ValuesRange& r1, 
            float v2, const ValuesRange& r2) 
  { 
    packedVal = (pack_float<T>(v2, r2.lo, r2.hi, num_bits2) << num_bits1) | pack_float<T>(v1, r1.lo, r1.hi, num_bits1); 
  }
  auto unpack(const ValuesRange& r1, const ValuesRange& r2) 
  { 
    return std::make_tuple(unpack_float<T>(packedVal & ((1 << num_bits1) - 1), r1.lo, r1.hi, num_bits1),
                           unpack_float<T>(packedVal >> num_bits1, r2.lo, r2.hi, num_bits2)); 
  }
};

template<typename T, int num_bits1, int num_bits2, int num_bits3>
struct PackedVec3
{
  T packedVal;

  PackedVec3(float v1, const ValuesRange& r1, 
             float v2, const ValuesRange& r2,
             float v3, const ValuesRange& r3) { pack(v1, r1, v2, r2, v3, r3); }
  PackedVec3(T compressed_val) : packedVal(compressed_val) {}

  void pack(float v1, const ValuesRange& r1, 
            float v2, const ValuesRange& r2,
            float v3, const ValuesRange& r3) 
  { 
    packedVal = (pack_float<T>(v3, r3.lo, r3.hi, num_bits3) << (num_bits1 + num_bits2)) |
                (pack_float<T>(v2, r2.lo, r2.hi, num_bits2) << num_bits1) | 
                 pack_float<T>(v1, r1.lo, r1.hi, num_bits1); 
  }
  auto unpack(const ValuesRange& r1, const ValuesRange& r2, const ValuesRange& r3) 
  { 
    return std::make_tuple(unpack_float<T>(packedVal & ((1 << num_bits1) - 1), r1.lo, r1.hi, num_bits1),
                           unpack_float<T>((packedVal >> num_bits1) & ((1 << num_bits2) - 1), r2.lo, r2.hi, num_bits2),
                           unpack_float<T>(packedVal >> (num_bits1 + num_bits2), r3.lo, r3.hi, num_bits3)); 
  }
};
