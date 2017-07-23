//
// Copyright (c) 2016-2017 Kris Jusiak (kris at jusiak dot net)
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/hana.hpp>

namespace hana = boost::hana;

using namespace hana::literals;

int main() {
  constexpr auto m = hana::make_map(
      hana::make_pair(1_c, 1_c), hana::make_pair(2_c, 2_c),
      hana::make_pair(3_c, 3_c), hana::make_pair(4_c, 4_c),
      hana::make_pair(5_c, 5_c), hana::make_pair(6_c, 6_c),
      hana::make_pair(7_c, 7_c), hana::make_pair(8_c, 8_c),
      hana::make_pair(9_c, 9_c), hana::make_pair(10_c, 10_c),
      hana::make_pair(11_c, 11_c), hana::make_pair(12_c, 12_c),
      hana::make_pair(13_c, 13_c), hana::make_pair(14_c, 14_c),
      hana::make_pair(15_c, 15_c), hana::make_pair(16_c, 16_c),
      hana::make_pair(17_c, 17_c), hana::make_pair(18_c, 18_c),
      hana::make_pair(19_c, 19_c), hana::make_pair(20_c, 20_c),
      hana::make_pair(21_c, 21_c), hana::make_pair(22_c, 22_c),
      hana::make_pair(23_c, 23_c), hana::make_pair(24_c, 24_c),
      hana::make_pair(25_c, 25_c), hana::make_pair(26_c, 26_c),
      hana::make_pair(27_c, 27_c), hana::make_pair(28_c, 28_c),
      hana::make_pair(29_c, 29_c), hana::make_pair(30_c, 30_c),
      hana::make_pair(31_c, 31_c), hana::make_pair(32_c, 32_c),
      hana::make_pair(33_c, 33_c), hana::make_pair(34_c, 34_c),
      hana::make_pair(35_c, 35_c), hana::make_pair(36_c, 36_c),
      hana::make_pair(37_c, 37_c), hana::make_pair(38_c, 38_c),
      hana::make_pair(39_c, 39_c), hana::make_pair(40_c, 40_c),
      hana::make_pair(41_c, 41_c), hana::make_pair(42_c, 42_c),
      hana::make_pair(43_c, 43_c), hana::make_pair(44_c, 44_c),
      hana::make_pair(45_c, 45_c), hana::make_pair(46_c, 46_c),
      hana::make_pair(47_c, 47_c), hana::make_pair(48_c, 48_c),
      hana::make_pair(49_c, 49_c), hana::make_pair(50_c, 50_c),
      hana::make_pair(51_c, 51_c), hana::make_pair(52_c, 52_c),
      hana::make_pair(53_c, 53_c), hana::make_pair(54_c, 54_c),
      hana::make_pair(55_c, 55_c), hana::make_pair(56_c, 56_c),
      hana::make_pair(57_c, 57_c), hana::make_pair(58_c, 58_c),
      hana::make_pair(59_c, 59_c), hana::make_pair(60_c, 60_c),
      hana::make_pair(61_c, 61_c), hana::make_pair(62_c, 62_c),
      hana::make_pair(63_c, 63_c), hana::make_pair(64_c, 64_c),
      hana::make_pair(65_c, 65_c), hana::make_pair(66_c, 66_c),
      hana::make_pair(67_c, 67_c), hana::make_pair(68_c, 68_c),
      hana::make_pair(69_c, 69_c), hana::make_pair(70_c, 70_c),
      hana::make_pair(71_c, 71_c), hana::make_pair(72_c, 72_c),
      hana::make_pair(73_c, 73_c), hana::make_pair(74_c, 74_c),
      hana::make_pair(75_c, 75_c), hana::make_pair(76_c, 76_c),
      hana::make_pair(77_c, 77_c), hana::make_pair(78_c, 78_c),
      hana::make_pair(79_c, 79_c), hana::make_pair(80_c, 80_c),
      hana::make_pair(81_c, 81_c), hana::make_pair(82_c, 82_c),
      hana::make_pair(83_c, 83_c), hana::make_pair(84_c, 84_c),
      hana::make_pair(85_c, 85_c), hana::make_pair(86_c, 86_c),
      hana::make_pair(87_c, 87_c), hana::make_pair(88_c, 88_c),
      hana::make_pair(89_c, 89_c), hana::make_pair(90_c, 90_c),
      hana::make_pair(91_c, 91_c), hana::make_pair(92_c, 92_c),
      hana::make_pair(93_c, 93_c), hana::make_pair(94_c, 94_c),
      hana::make_pair(95_c, 95_c), hana::make_pair(96_c, 96_c),
      hana::make_pair(97_c, 97_c), hana::make_pair(98_c, 98_c),
      hana::make_pair(99_c, 99_c), hana::make_pair(100_c, 100_c),
      hana::make_pair(101_c, 101_c), hana::make_pair(102_c, 102_c),
      hana::make_pair(103_c, 103_c), hana::make_pair(104_c, 104_c),
      hana::make_pair(105_c, 105_c), hana::make_pair(106_c, 106_c),
      hana::make_pair(107_c, 107_c), hana::make_pair(108_c, 108_c),
      hana::make_pair(109_c, 109_c), hana::make_pair(110_c, 110_c),
      hana::make_pair(111_c, 111_c), hana::make_pair(112_c, 112_c),
      hana::make_pair(113_c, 113_c), hana::make_pair(114_c, 114_c),
      hana::make_pair(115_c, 115_c), hana::make_pair(116_c, 116_c),
      hana::make_pair(117_c, 117_c), hana::make_pair(118_c, 118_c),
      hana::make_pair(119_c, 119_c), hana::make_pair(120_c, 120_c),
      hana::make_pair(121_c, 121_c), hana::make_pair(122_c, 122_c),
      hana::make_pair(123_c, 123_c), hana::make_pair(124_c, 124_c),
      hana::make_pair(125_c, 125_c), hana::make_pair(126_c, 126_c),
      hana::make_pair(127_c, 127_c), hana::make_pair(128_c, 128_c));

  static_assert(m[1_c] == 1_c, "");
  static_assert(m[2_c] == 2_c, "");
  static_assert(m[3_c] == 3_c, "");
  static_assert(m[4_c] == 4_c, "");
  static_assert(m[5_c] == 5_c, "");
  static_assert(m[6_c] == 6_c, "");
  static_assert(m[7_c] == 7_c, "");
  static_assert(m[8_c] == 8_c, "");
  static_assert(m[9_c] == 9_c, "");
  static_assert(m[10_c] == 10_c, "");
  static_assert(m[11_c] == 11_c, "");
  static_assert(m[12_c] == 12_c, "");
  static_assert(m[13_c] == 13_c, "");
  static_assert(m[14_c] == 14_c, "");
  static_assert(m[15_c] == 15_c, "");
  static_assert(m[16_c] == 16_c, "");
  static_assert(m[17_c] == 17_c, "");
  static_assert(m[18_c] == 18_c, "");
  static_assert(m[19_c] == 19_c, "");
  static_assert(m[20_c] == 20_c, "");
  static_assert(m[21_c] == 21_c, "");
  static_assert(m[22_c] == 22_c, "");
  static_assert(m[23_c] == 23_c, "");
  static_assert(m[24_c] == 24_c, "");
  static_assert(m[25_c] == 25_c, "");
  static_assert(m[26_c] == 26_c, "");
  static_assert(m[27_c] == 27_c, "");
  static_assert(m[28_c] == 28_c, "");
  static_assert(m[29_c] == 29_c, "");
  static_assert(m[30_c] == 30_c, "");
  static_assert(m[31_c] == 31_c, "");
  static_assert(m[32_c] == 32_c, "");
  static_assert(m[33_c] == 33_c, "");
  static_assert(m[34_c] == 34_c, "");
  static_assert(m[35_c] == 35_c, "");
  static_assert(m[36_c] == 36_c, "");
  static_assert(m[37_c] == 37_c, "");
  static_assert(m[38_c] == 38_c, "");
  static_assert(m[39_c] == 39_c, "");
  static_assert(m[40_c] == 40_c, "");
  static_assert(m[41_c] == 41_c, "");
  static_assert(m[42_c] == 42_c, "");
  static_assert(m[43_c] == 43_c, "");
  static_assert(m[44_c] == 44_c, "");
  static_assert(m[45_c] == 45_c, "");
  static_assert(m[46_c] == 46_c, "");
  static_assert(m[47_c] == 47_c, "");
  static_assert(m[48_c] == 48_c, "");
  static_assert(m[49_c] == 49_c, "");
  static_assert(m[50_c] == 50_c, "");
  static_assert(m[51_c] == 51_c, "");
  static_assert(m[52_c] == 52_c, "");
  static_assert(m[53_c] == 53_c, "");
  static_assert(m[54_c] == 54_c, "");
  static_assert(m[55_c] == 55_c, "");
  static_assert(m[56_c] == 56_c, "");
  static_assert(m[57_c] == 57_c, "");
  static_assert(m[58_c] == 58_c, "");
  static_assert(m[59_c] == 59_c, "");
  static_assert(m[60_c] == 60_c, "");
  static_assert(m[61_c] == 61_c, "");
  static_assert(m[62_c] == 62_c, "");
  static_assert(m[63_c] == 63_c, "");
  static_assert(m[64_c] == 64_c, "");
  static_assert(m[65_c] == 65_c, "");
  static_assert(m[66_c] == 66_c, "");
  static_assert(m[67_c] == 67_c, "");
  static_assert(m[68_c] == 68_c, "");
  static_assert(m[69_c] == 69_c, "");
  static_assert(m[70_c] == 70_c, "");
  static_assert(m[71_c] == 71_c, "");
  static_assert(m[72_c] == 72_c, "");
  static_assert(m[73_c] == 73_c, "");
  static_assert(m[74_c] == 74_c, "");
  static_assert(m[75_c] == 75_c, "");
  static_assert(m[76_c] == 76_c, "");
  static_assert(m[77_c] == 77_c, "");
  static_assert(m[78_c] == 78_c, "");
  static_assert(m[79_c] == 79_c, "");
  static_assert(m[80_c] == 80_c, "");
  static_assert(m[81_c] == 81_c, "");
  static_assert(m[82_c] == 82_c, "");
  static_assert(m[83_c] == 83_c, "");
  static_assert(m[84_c] == 84_c, "");
  static_assert(m[85_c] == 85_c, "");
  static_assert(m[86_c] == 86_c, "");
  static_assert(m[87_c] == 87_c, "");
  static_assert(m[88_c] == 88_c, "");
  static_assert(m[89_c] == 89_c, "");
  static_assert(m[90_c] == 90_c, "");
  static_assert(m[91_c] == 91_c, "");
  static_assert(m[92_c] == 92_c, "");
  static_assert(m[93_c] == 93_c, "");
  static_assert(m[94_c] == 94_c, "");
  static_assert(m[95_c] == 95_c, "");
  static_assert(m[96_c] == 96_c, "");
  static_assert(m[97_c] == 97_c, "");
  static_assert(m[98_c] == 98_c, "");
  static_assert(m[99_c] == 99_c, "");
  static_assert(m[100_c] == 100_c, "");
  static_assert(m[101_c] == 101_c, "");
  static_assert(m[102_c] == 102_c, "");
  static_assert(m[103_c] == 103_c, "");
  static_assert(m[104_c] == 104_c, "");
  static_assert(m[105_c] == 105_c, "");
  static_assert(m[106_c] == 106_c, "");
  static_assert(m[107_c] == 107_c, "");
  static_assert(m[108_c] == 108_c, "");
  static_assert(m[109_c] == 109_c, "");
  static_assert(m[110_c] == 110_c, "");
  static_assert(m[111_c] == 111_c, "");
  static_assert(m[112_c] == 112_c, "");
  static_assert(m[113_c] == 113_c, "");
  static_assert(m[114_c] == 114_c, "");
  static_assert(m[115_c] == 115_c, "");
  static_assert(m[116_c] == 116_c, "");
  static_assert(m[117_c] == 117_c, "");
  static_assert(m[118_c] == 118_c, "");
  static_assert(m[119_c] == 119_c, "");
  static_assert(m[120_c] == 120_c, "");
  static_assert(m[121_c] == 121_c, "");
  static_assert(m[122_c] == 122_c, "");
  static_assert(m[123_c] == 123_c, "");
  static_assert(m[124_c] == 124_c, "");
  static_assert(m[125_c] == 125_c, "");
  static_assert(m[126_c] == 126_c, "");
  static_assert(m[127_c] == 127_c, "");
  static_assert(m[128_c] == 128_c, "");
}