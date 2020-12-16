/*{
Copyright © 2009-2019 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
                      Matthias Kretz <m.kretz@gsi.de>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the names of contributing organizations nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

}*/

//#define _GLIBCXX_SIMD_DEBUG frexp
//#define UNITTEST_ONLY_XTEST 1
#include "unittest.h"
#include "metahelpers.h"
#include <cmath>    // abs & sqrt
#include <cstdlib>  // integer abs
#include "mathreference.h"
#include "simd_view.h"
#include "test_values.h"

template <class... Ts> using base_template = std::experimental::simd<Ts...>;
#include "testtypes.h"

template <size_t Offset, class V, class Iterator>
  V
  test_tuples_gather(const Iterator& it)
  {
    return V([&](auto i) { return it[i][Offset]; });
  }

template <size_t Offset, class V, class Iterator>
  V
  test_tuples_gather_epilogue(const Iterator& it, const size_t remaining)
  {
    return V([&](auto i) { return it[i < remaining ? i : 0][Offset]; });
  }

template <class V, size_t N, size_t... Indexes, class... F>
  void
  test_tuples_impl(
    std::index_sequence<Indexes...>,
    const std::initializer_list<std::array<typename V::value_type, N>>& data,
    F&&... fun_pack)
  {
    auto it = data.begin();
    for (; it + V::size() <= data.end(); it += V::size())
      {
	[](auto...) {}((fun_pack(test_tuples_gather<Indexes, V>(it)...), 0)...);
      }
    const auto remaining = data.size() % V::size();
    if (remaining > 0)
      {
	[](auto...) {}((fun_pack(test_tuples_gather_epilogue<Indexes, V>(
		it, data.size() % V::size())...),
		       0)...);
      }
  }

template <class V, size_t N, class... F>
  void
  test_tuples(
    const std::initializer_list<std::array<typename V::value_type, N>>& data,
    F&&... fun_pack)
  {
    test_tuples_impl<V, N>(std::make_index_sequence<N>(), data,
			   static_cast<F&&>(fun_pack)...);
  }

TEST_TYPES(V, abs, all_test_types)
  {
    if constexpr (std::is_signed_v<typename V::value_type>)
      {
	using std::abs;
	using T = typename V::value_type;
	test_values<V>({std::__finite_max_v<T>, std::__norm_min_v<T>,
			-std::__norm_min_v<T>, std::__finite_min_v<T>,
			std::__finite_min_v<T> / 2, T(), -T(), T(-1), T(-2)},
		       {1000}, [](V input) {
			 const V expected(
			   [&](auto i) { return T(std::abs(T(input[i]))); });
			 COMPARE(abs(input), expected) << "input: " << input;
		       });
      }
  }

template <typename F>
  auto
  verify_no_fp_exceptions(F&& fun)
  {
    std::feclearexcept(FE_ALL_EXCEPT);
    auto r = fun();
    COMPARE(std::fetestexcept(FE_ALL_EXCEPT), 0);
    return r;
  }

#define NOFPEXCEPT(...) verify_no_fp_exceptions([&]() { return __VA_ARGS__; })

TEST_TYPES(V, fpclassify, real_test_types)
  {
    using T = typename V::value_type;
    using intv = std::experimental::fixed_size_simd<int, V::size()>;
    constexpr T inf = std::__infinity_v<T>;
    constexpr T denorm_min = std::__infinity_v<T>;
    constexpr T nan = std::__quiet_NaN_v<T>;
    constexpr T max = std::__finite_max_v<T>;
    constexpr T norm_min = std::__norm_min_v<T>;
    test_values<V>(
      {0., 1., -1.,
#if __GCC_IEC_559 >= 2
       -0., inf, -inf, denorm_min, -denorm_min, nan,
       norm_min * 0.9, -norm_min * 0.9,
#endif
       max, -max, norm_min, -norm_min
      },
      [](const V input) {
	COMPARE(NOFPEXCEPT(isfinite(input)),
		!V([&](auto i) { return std::isfinite(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isinf(input)),
		!V([&](auto i) { return std::isinf(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isnan(input)),
		!V([&](auto i) { return std::isnan(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isnormal(input)),
		!V([&](auto i) { return std::isnormal(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(signbit(input)),
		!V([&](auto i) { return std::signbit(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isunordered(input, V())),
		!V([&](auto i) { return std::isunordered(input[i], 0) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isunordered(V(), input)),
		!V([&](auto i) { return std::isunordered(0, input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(fpclassify(input)),
		intv([&](auto i) { return std::fpclassify(input[i]); }))
	  << input;
      });
#ifdef __SUPPORT_SNAN__
    const V snan = std::__signaling_NaN_v<T>;
    COMPARE(isfinite(snan),
	    !V([&](auto i) { return std::isfinite(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(isinf(snan), !V([&](auto i) { return std::isinf(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(isnan(snan), !V([&](auto i) { return std::isnan(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(isnormal(snan),
	    !V([&](auto i) { return std::isnormal(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(signbit(snan),
	    !V([&](auto i) { return std::signbit(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(isunordered(snan, V()),
	    !V([&](auto i) { return std::isunordered(snan[i], 0) ? 0 : 1; }))
      << snan;
    COMPARE(isunordered(V(), snan),
	    !V([&](auto i) { return std::isunordered(0, snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(fpclassify(snan),
	    intv([&](auto i) { return std::fpclassify(snan[i]); }))
      << snan;
#endif
  }

TEST_TYPES(V, trunc_ceil_floor, real_test_types)
  {
    using T = typename V::value_type;
    constexpr T inf = std::__infinity_v<T>;
    constexpr T denorm_min = std::__denorm_min_v<T>;
    constexpr T norm_min = std::__norm_min_v<T>;
    constexpr T max = std::__finite_max_v<T>;
    constexpr T min = std::__finite_min_v<T>;
    test_values<V>(
      {2.1,
       2.0,
       2.9,
       2.5,
       2.499,
       1.5,
       1.499,
       1.99,
       0.99,
       0.5,
       0.499,
       0.,
       -2.1,
       -2.0,
       -2.9,
       -2.5,
       -2.499,
       -1.5,
       -1.499,
       -1.99,
       -0.99,
       -0.5,
       -0.499,
       3 << 21,
       3 << 22,
       3 << 23,
       -(3 << 21),
       -(3 << 22),
       -(3 << 23),
#ifdef __STDC_IEC_559__
       -0.,
       inf,
       -inf,
       denorm_min,
       norm_min * 0.9,
       -denorm_min,
       -norm_min * 0.9,
#endif
       max,
       norm_min,
       min,
       -norm_min
      },
      [](const V input) {
	const V expected([&](auto i) { return std::trunc(input[i]); });
	COMPARE(trunc(input), expected) << input;
      },
      [](const V input) {
	const V expected([&](auto i) { return std::ceil(input[i]); });
	COMPARE(ceil(input), expected) << input;
      },
      [](const V input) {
	const V expected([&](auto i) { return std::floor(input[i]); });
	COMPARE(floor(input), expected) << input;
      });

#ifdef __STDC_IEC_559__
    test_values<V>(
      {
#ifdef __SUPPORT_SNAN__
	std::__signaling_NaN_v<T>,
#endif
	std::__quiet_NaN_v<T>},
      [](const V input) {
	const V expected([&](auto i) { return std::trunc(input[i]); });
	COMPARE(isnan(trunc(input)), isnan(expected)) << input;
      },
      [](const V input) {
	const V expected([&](auto i) { return std::ceil(input[i]); });
	COMPARE(isnan(ceil(input)), isnan(expected)) << input;
      },
      [](const V input) {
	const V expected([&](auto i) { return std::floor(input[i]); });
	COMPARE(isnan(floor(input)), isnan(expected)) << input;
      });
#endif
  }

TEST_TYPES(V, frexp, real_test_types)
  {
    using int_v = std::experimental::fixed_size_simd<int, V::size()>;
    using T = typename V::value_type;
    constexpr auto denorm_min = std::__denorm_min_v<T>;
    constexpr auto norm_min = std::__norm_min_v<T>;
    constexpr auto max = std::__finite_max_v<T>;
    constexpr auto nan = std::__quiet_NaN_v<T>;
    constexpr auto inf = std::__infinity_v<T>;
    test_values<V>(
      {0, 0.25, 0.5, 1, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
       20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 32, 31, -0., -0.25, -0.5, -1,
       -3, -4, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16, -17, -18,
       -19, -20, -21, -22, -23, -24, -25, -26, -27, -28, -29, -32, -31,
#if __GCC_IEC_559 >= 2
       denorm_min, -denorm_min, norm_min / 2, -norm_min / 2,
#endif
       max, -max, max * 0.123f, -max * 0.123f},
      [](const V input) {
	V expectedFraction;
	const int_v expectedExponent([&](auto i) {
	  int exp;
	  expectedFraction[i] = std::frexp(input[i], &exp);
	  return exp;
	});
	int_v exponent = {};
	const V fraction = frexp(input, &exponent);
	COMPARE(fraction, expectedFraction) << ", input = " << input
	  << ", delta: " << fraction - expectedFraction;
	COMPARE(exponent, expectedExponent)
	  << "\ninput: " << input << ", fraction: " << fraction;
      });
#ifdef __STDC_IEC_559__
    test_values<V>(
      // If x is a NaN, a NaN is returned, and the value of *exp is unspecified.
      //
      // If x is positive  infinity  (negative  infinity),  positive  infinity
      // (negative infinity) is returned, and the value of *exp is unspecified.
      // This behavior is only guaranteed with C's Annex F when __STDC_IEC_559__
      // is defined.
      {nan, inf, -inf, denorm_min, denorm_min * 1.72, -denorm_min,
       -denorm_min * 1.72, 0., -0., 1, -1},
      [](const V input) {
	const V expectedFraction([&](auto i) {
	  int exp;
	  return std::frexp(input[i], &exp);
	});
	int_v exponent = {};
	const V fraction = frexp(input, &exponent);
	COMPARE(isnan(fraction), isnan(expectedFraction))
	  << fraction << ", input = " << input
	  << ", delta: " << fraction - expectedFraction;
	COMPARE(isinf(fraction), isinf(expectedFraction))
	  << fraction << ", input = " << input
	  << ", delta: " << fraction - expectedFraction;
	COMPARE(signbit(fraction), signbit(expectedFraction))
	  << fraction << ", input = " << input
	  << ", delta: " << fraction - expectedFraction;
      });
#endif
  }

TEST_TYPES(V, sincos, real_test_types)
  {
    using std::cos;
    using std::sin;
    using T = typename V::value_type;

    vir::test::setFuzzyness<float>(2);
    vir::test::setFuzzyness<double>(1);

    const auto& testdata = referenceData<function::sincos, T>();
    std::experimental::experimental::simd_view<V>(testdata).for_each(
      [&](const V input, const V expected_sin, const V expected_cos) {
	FUZZY_COMPARE(sin(input), expected_sin) << " input = " << input;
	FUZZY_COMPARE(sin(-input), -expected_sin) << " input = " << input;
	FUZZY_COMPARE(cos(input), expected_cos) << " input = " << input;
	FUZZY_COMPARE(cos(-input), expected_cos) << " input = " << input;
      });
  }

TEST_TYPES(V, asin, real_test_types)
  {
    using std::asin;
    using T = typename V::value_type;

    vir::test::setFuzzyness<float>(2);
    vir::test::setFuzzyness<double>(36);

    const auto& testdata = referenceData<function::asin, T>();
    std::experimental::experimental::simd_view<V>(testdata).for_each(
      [&](const V input, const V expected) {
	FUZZY_COMPARE(asin(input), expected) << " input = " << input;
	FUZZY_COMPARE(asin(-input), -expected) << " input = " << input;
      });
  }

TEST_TYPES(V, atan, real_test_types)
  {
    using std::atan;
    using T = typename V::value_type;

    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);

    using limits = std::numeric_limits<typename V::value_type>;
    test_values<V>({limits::quiet_NaN(), limits::infinity(), -limits::infinity()},
		   [](const V input) {
		     const V expected(
		       [&](auto i) { return std::atan(input[i]); });
		     COMPARE(isnan(atan(input)), isnan(expected));
		     const V clean = iif(isnan(input), 0, input);
		     COMPARE(atan(clean),
			     V([&](auto i) { return std::atan(clean[i]); }));
		   });

    const auto& testdata = referenceData<function::atan, T>();
    std::experimental::experimental::simd_view<V>(testdata).for_each(
      [&](const V input, const V expected) {
	FUZZY_COMPARE(atan(input), expected) << " input = " << input;
	FUZZY_COMPARE(atan(-input), -expected) << " input = " << input;
      });
  }

TEST_TYPES(V, atan2, real_test_types)
  {
    using std::atan2;
    using T = typename V::value_type;

    vir::test::setFuzzyness<float>(3);
    vir::test::setFuzzyness<double>(2);

    using limits = std::numeric_limits<typename V::value_type>;
    const T Pi = 0xc.90fdaa22168c235p-2L;
    const T inf = limits::infinity();
    test_tuples<V, 3>(
      {
	// If y is +0 (-0) and x is less than 0, +pi (-pi) is returned.
	{+0., -3., +Pi},
	{-0., -3., -Pi},
	// If y is +0 (-0) and x is greater than 0, +0 (-0) is returned.
	{+0., +3., +0.},
	{-0., +3., -0.},
	// If y is less than 0 and x is +0 or -0, -pi/2 is returned.
	{-3., +0., -Pi / 2},
	{-3., -0., -Pi / 2},
	// If y is greater than 0 and x is +0 or -0, pi/2 is returned.
	{+3., +0., +Pi / 2},
	{+3., -0., +Pi / 2},
	// If y is +0 (-0 and x is -0, +pi (-pi is returned.
	{+0., -0., +Pi},
	{-0., -0., -Pi},
	// If y is +0 (-0 and x is +0, +0 (-0 is returned.
	{+0., +0., +0.},
	{-0., +0., -0.},
	// If y is a finite value greater (less than 0, and x is negative
	// infinity, +pi
	// (-pi is returned.
	{+1., -inf, +Pi},
	{-1., -inf, -Pi},
	// If y is a finite value greater (less than 0, and x is positive
	// infinity, +0
	// (-0 is returned.
	{+3., +inf, +0.},
	{-3., +inf, -0.},
	// If y is positive infinity (negative infinity, and x is finite, pi/2
	// (-pi/2 is returned.
	{+inf, +3., +Pi / 2},
	{-inf, +3., -Pi / 2},
	{+inf, -3., +Pi / 2},
	{-inf, -3., -Pi / 2},
#ifndef _WIN32 // the Microsoft implementation of atan2 fails this test
	// If y is positive infinity (negative infinity) and x is negative
	// infinity, +3*pi/4 (-3*pi/4) is returned.
	{+inf, -inf, T(+3. * (Pi / 4))},
	{-inf, -inf, T(-3. * (Pi / 4))},
	// If y is positive infinity (negative infinity) and x is positive
	// infinity, +pi/4 (-pi/4) is returned.
	{+inf, +inf, +Pi / 4},
	{-inf, +inf, -Pi / 4},
#endif
	// If either x or y is NaN, a NaN is returned.
	{limits::quiet_NaN(), 3., limits::quiet_NaN()},
	{3., limits::quiet_NaN(), limits::quiet_NaN()},
	{limits::quiet_NaN(), limits::quiet_NaN(), limits::quiet_NaN()},
      },
      [](V x, V y, V expected) {
	COMPARE(isnan(atan2(x, y)), isnan(expected)) << x << y;
	where(isnan(expected), x) = 0;
	where(isnan(expected), y) = 0;
	where(isnan(expected), expected) = 0;
	COMPARE(atan2(x, y), expected) << x << y;
	COMPARE(signbit(atan2(x, y)), signbit(expected)) << x << y;
      });

    for (int xoffset = -100; xoffset < 54613; xoffset += 47 * int(V::size()))
      {
	for (int yoffset = -100; yoffset < 54613; yoffset += 47 * int(V::size()))
	  {
	    const V data = V([](T i) { return i; });
	    const V reference = V([&](auto i) {
	      return std::atan2((data[i] + xoffset) * T(0.15),
				(data[i] + yoffset) * T(0.15));
	    });

	    const V x = (data + xoffset) * T(0.15);
	    const V y = (data + yoffset) * T(0.15);
	    FUZZY_COMPARE(atan2(x, y), reference)
	      << ", x = " << x << ", y = " << y;
	  }
      }
  }

TEST_TYPES(V, trig, real_test_types)
  {
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);

    using T = typename V::value_type;
    test_values<V>(
      {
#ifdef __STDC_IEC_559__
	std::__quiet_NaN_v<T>, std::__infinity_v<T>, -std::__infinity_v<T>, -0.,
	std::__denorm_min_v<T>, std::__norm_min_v<T> / 3,
#endif
	+0., std::__norm_min_v<T>, std::__finite_max_v<T>},
      {10000}, MAKE_TESTER(acos), MAKE_TESTER(tan), MAKE_TESTER(acosh),
      MAKE_TESTER(asinh), MAKE_TESTER(atanh), MAKE_TESTER(cosh),
      MAKE_TESTER(sinh), MAKE_TESTER(tanh));
  }

TEST_TYPES(V, logarithms, real_test_types)
  {
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);

    using T = typename V::value_type;
    constexpr T nan = std::__quiet_NaN_v<T>;
    constexpr T inf = std::__infinity_v<T>;
    constexpr T denorm_min = std::__denorm_min_v<T>;
    constexpr T norm_min = std::__norm_min_v<T>;
    constexpr T min = std::__finite_min_v<T>;
    constexpr T max = std::__finite_max_v<T>;
    test_values<V>({1,
		    2,
		    4,
		    8,
		    16,
		    32,
		    64,
		    128,
		    256,
		    512,
		    1024,
		    2048,
		    3,
		    5,
		    7,
		    15,
		    17,
		    31,
		    33,
		    63,
		    65,
#ifdef __STDC_IEC_559__
		    nan,
		    inf,
		    -inf,
		    denorm_min,
		    -denorm_min,
		    norm_min / 3,
		    -norm_min / 3,
		    -T(),
		    -norm_min,
		    min,
		    T(),
#endif
		    norm_min,
		    max},
		   {10000,
#ifdef __STDC_IEC_559__
		    min / 2,
#else
		    norm_min,
#endif
		    max / 2},
		   MAKE_TESTER(log), MAKE_TESTER(log10), MAKE_TESTER(log1p),
		   MAKE_TESTER(log2), MAKE_TESTER(logb));
  }

TEST_TYPES(V, exponentials, real_test_types)
  {
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);

    using limits = std::numeric_limits<typename V::value_type>;
    test_values<V>({limits::quiet_NaN(), limits::infinity(), -limits::infinity(),
		    +0., -0., limits::denorm_min(), limits::min(), limits::max(),
		    limits::min() / 3},
		   {10000, -limits::max() / 2, limits::max() / 2},
		   MAKE_TESTER(exp), MAKE_TESTER(exp2), MAKE_TESTER(expm1));
  }

TEST_TYPES(V, test1Arg, real_test_types)
  {
    vir::test::setFuzzyness<float>(0);
    vir::test::setFuzzyness<double>(0);

    using T = typename V::value_type;
    constexpr T inf = std::__infinity_v<T>;
    constexpr T nan = std::__quiet_NaN_v<T>;
    constexpr T denorm_min = std::__denorm_min_v<T>;
    constexpr T norm_min = std::__norm_min_v<T>;
    constexpr T max = std::__finite_max_v<T>;
#if defined __LONG_DOUBLE_IBM128__
    // On POWER with IBM128 long double, 1+eps and 2-eps is not a constant
    // expression. Until this is fixed, just use const instead of constexpr.
    // (error: '(1.0e+0l + 4.94065645841246544176568792868221e-324l)' is not a
    // constant expression)
    const T after_one = 1 + std::__epsilon_v<T>;
    const T before_one = (2 - std::__epsilon_v<T>) / 2;
#else
    constexpr T after_one = 1 + std::__epsilon_v<T>;
    constexpr T before_one = (2 - std::__epsilon_v<T>) / 2;
#endif
    const std::initializer_list<T>
      input_values = {+0.,
		      0.5,
		      -0.5,
		      before_one,
		      -before_one,
		      after_one,
		      -after_one,
		      1.5,
		      -1.5,
		      2 * before_one,
		      -2 * before_one,
		      2 * after_one,
		      -2 * after_one,
		      2.5,
		      -2.5,
		      0x1.fffffffffffffp52,
		      -0x1.fffffffffffffp52,
		      0x1.ffffffffffffep52,
		      -0x1.ffffffffffffep52,
		      0x1.ffffffffffffdp52,
		      -0x1.ffffffffffffdp52,
		      0x1.fffffep21,
		      -0x1.fffffep21,
		      0x1.fffffcp21,
		      -0x1.fffffcp21,
		      0x1.fffffep22,
		      -0x1.fffffep22,
		      0x1.fffffcp22,
		      -0x1.fffffcp22,
		      0x1.fffffep23,
		      -0x1.fffffep23,
		      0x1.fffffcp23,
		      -0x1.fffffcp23,
		      0x1.8p23,
		      -0x1.8p23,
		      inf,
		      -inf,
		      -0.,
		      nan,
		      denorm_min,
		      norm_min / 3,
		      norm_min,
		      max};
    test_values<V>(input_values, {10000}, MAKE_TESTER(erf), MAKE_TESTER(erfc),
		   MAKE_TESTER(tgamma), MAKE_TESTER(lgamma), MAKE_TESTER(ceil),
		   MAKE_TESTER(floor), MAKE_TESTER(trunc), MAKE_TESTER(round),
		   MAKE_TESTER(lround), MAKE_TESTER(llround),
		   MAKE_TESTER(nearbyint), MAKE_TESTER(rint), MAKE_TESTER(lrint),
		   MAKE_TESTER(llrint), MAKE_TESTER(ilogb));

    // sqrt(x) on x87 is precise in 80 bits, but the subsequent rounding can be
    // wrong (up to 1 ULP)
#if __FLT_EVAL_METHOD__ == 1
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(0);
#elif __FLT_EVAL_METHOD__ == 2
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);
#endif
    test_values<V>(input_values, {10000}, MAKE_TESTER(sqrt));
  }

TEST_TYPES(V, test2Arg, real_test_types)
  {
    using T = typename V::value_type;

    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);
    vir::test::setFuzzyness<long double>(1);
    test_values_2arg<V>(
      {
#ifdef __STDC_IEC_559__
	std::__quiet_NaN_v<T>, std::__infinity_v<T>, -std::__infinity_v<T>, -0.,
	std::__denorm_min_v<T>, std::__norm_min_v<T> / 3,
#endif
	+0., std::__norm_min_v<T>, 1., 2., std::__finite_max_v<T> / 5,
	std::__finite_max_v<T> / 3, std::__finite_max_v<T> / 2,
#ifdef __FAST_MATH__
	// fast-math hypot is imprecise for the max exponent
      },
      {100000, std::__finite_max_v<T> / 2},
#else
	std::__finite_max_v<T>},
      {100000},
#endif
      MAKE_TESTER(hypot));
#if !__FINITE_MATH_ONLY__
    COMPARE(hypot(V(std::__finite_max_v<T>), V(std::__finite_max_v<T>)),
	    V(std::__infinity_v<T>));
#endif
    COMPARE(hypot(V(std::__norm_min_v<T>), V(std::__norm_min_v<T>)),
	    V(std::__norm_min_v<T> * std::sqrt(T(2))));
    VERIFY((sfinae_is_callable<V, V>(
	  [](auto a, auto b) -> decltype(hypot(a, b)) { return {}; })));
    VERIFY((sfinae_is_callable<typename V::value_type, V>(
	  [](auto a, auto b) -> decltype(hypot(a, b)) { return {}; })));
    VERIFY((sfinae_is_callable<V, typename V::value_type>(
	  [](auto a, auto b) -> decltype(hypot(a, b)) { return {}; })));

    vir::test::setFuzzyness<float>(0);
    vir::test::setFuzzyness<double>(0);
    vir::test::setFuzzyness<long double>(0);
    test_values_2arg<V>(
      {
#ifdef __STDC_IEC_559__
	std::__quiet_NaN_v<T>, std::__infinity_v<T>, -std::__infinity_v<T>,
	std::__denorm_min_v<T>, std::__norm_min_v<T> / 3, -0.,
#endif
	+0., std::__norm_min_v<T>, std::__finite_max_v<T>},
      {10000}, MAKE_TESTER(pow), MAKE_TESTER(fmod), MAKE_TESTER(remainder),
      MAKE_TESTER_NOFPEXCEPT(copysign),
      MAKE_TESTER(nextafter), // MAKE_TESTER(nexttoward),
      MAKE_TESTER(fdim), MAKE_TESTER(fmax), MAKE_TESTER(fmin),
      MAKE_TESTER_NOFPEXCEPT(isgreater), MAKE_TESTER_NOFPEXCEPT(isgreaterequal),
      MAKE_TESTER_NOFPEXCEPT(isless), MAKE_TESTER_NOFPEXCEPT(islessequal),
      MAKE_TESTER_NOFPEXCEPT(islessgreater), MAKE_TESTER_NOFPEXCEPT(isunordered));
  }

// 3-arg std::hypot needs to be fixed, this is a better reference:
template <typename T>
  [[gnu::optimize("-fno-unsafe-math-optimizations")]]
  T
  hypot3(T x, T y, T z)
  {
    x = std::abs(x);
    y = std::abs(y);
    z = std::abs(z);
    if (std::isinf(x) || std::isinf(y) || std::isinf(z))
      return std::__infinity_v<T>;
    else if (std::isnan(x) || std::isnan(y) || std::isnan(z))
      return std::__quiet_NaN_v<T>;
    else if (x == y && y == z)
      return x * std::sqrt(T(3));
    else if (z == 0 && y == 0)
      return x;
    else if (x == 0 && z == 0)
      return y;
    else if (x == 0 && y == 0)
      return z;
    else
      {
	T hi = std::max(std::max(x, y), z);
	T lo0 = std::min(std::max(x, y), z);
	T lo1 = std::min(x, y);
	int e = 0;
	hi = std::frexp(hi, &e);
	lo0 = std::ldexp(lo0, -e);
	lo1 = std::ldexp(lo1, -e);
	T lo = lo0 * lo0 + lo1 * lo1;
	return std::ldexp(std::sqrt(hi * hi + lo), e);
      }
  }

TEST_TYPES(V, hypot3_fma, real_test_types)
  {
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);
    vir::test::setFuzzyness<long double>(2); // because of the bad reference

    using T = typename V::value_type;
    test_values_3arg<V>(
      {
#ifdef __STDC_IEC_559__
	std::__quiet_NaN_v<T>,
	std::__infinity_v<T>,
	-std::__infinity_v<T>,
	std::__norm_min_v<T> / 3,
	-0.,
	std::__denorm_min_v<T>,
#endif
	0.,
	1.,
	-1.,
	std::__norm_min_v<T>,
	-std::__norm_min_v<T>,
	2.,
	-2.,
	std::__finite_max_v<T> / 5,
	std::__finite_max_v<T> / 3,
	std::__finite_max_v<T> / 2,
	-std::__finite_max_v<T> / 5,
	-std::__finite_max_v<T> / 3,
	-std::__finite_max_v<T> / 2,
#ifdef __FAST_MATH__
	// fast-math hypot is imprecise for the max exponent
      },
      {100000, std::__finite_max_v<T> / 2},
#else
	std::__finite_max_v<T>, -std::__finite_max_v<T>},
      {100000},
#endif
      MAKE_TESTER_2(hypot, hypot3));
#if !__FINITE_MATH_ONLY__
    COMPARE(hypot(V(std::__finite_max_v<T>), V(std::__finite_max_v<T>), V()),
	    V(std::__infinity_v<T>));
    COMPARE(hypot(V(std::__finite_max_v<T>), V(), V(std::__finite_max_v<T>)),
	    V(std::__infinity_v<T>));
    COMPARE(hypot(V(), V(std::__finite_max_v<T>), V(std::__finite_max_v<T>)),
	    V(std::__infinity_v<T>));
#endif
    COMPARE(hypot(V(std::__norm_min_v<T>), V(std::__norm_min_v<T>),
		  V(std::__norm_min_v<T>)),
	    V(std::__norm_min_v<T> * std::sqrt(T(3))));
    auto&& hypot3_test
      = [](auto a, auto b, auto c) -> decltype(hypot(a, b, c)) { return {}; };
    VERIFY((sfinae_is_callable<V, V, V>(hypot3_test)));
    VERIFY((sfinae_is_callable<T, T, V>(hypot3_test)));
    VERIFY((sfinae_is_callable<V, T, T>(hypot3_test)));
    VERIFY((sfinae_is_callable<T, V, T>(hypot3_test)));
    VERIFY((sfinae_is_callable<T, V, V>(hypot3_test)));
    VERIFY((sfinae_is_callable<V, T, V>(hypot3_test)));
    VERIFY((sfinae_is_callable<V, V, T>(hypot3_test)));
    VERIFY((sfinae_is_callable<int, int, V>(hypot3_test)));
    VERIFY((sfinae_is_callable<int, V, int>(hypot3_test)));
    VERIFY((sfinae_is_callable<V, T, int>(hypot3_test)));
    VERIFY(!(sfinae_is_callable<bool, V, V>(hypot3_test)));
    VERIFY(!(sfinae_is_callable<V, bool, V>(hypot3_test)));
    VERIFY(!(sfinae_is_callable<V, V, bool>(hypot3_test)));

    vir::test::setFuzzyness<float>(0);
    vir::test::setFuzzyness<double>(0);
    test_values_3arg<V>(
      {
#ifdef __STDC_IEC_559__
	std::__quiet_NaN_v<T>, std::__infinity_v<T>, -std::__infinity_v<T>, -0.,
	std::__norm_min_v<T> / 3, std::__denorm_min_v<T>,
#endif
	0., std::__norm_min_v<T>, std::__finite_max_v<T>},
      {10000, -std::__finite_max_v<T> / 2, std::__finite_max_v<T> / 2},
      MAKE_TESTER(fma));
    auto&& fma_test
      = [](auto a, auto b, auto c) -> decltype(fma(a, b, c)) { return {}; };
    VERIFY((sfinae_is_callable<V, V, V>(fma_test)));
    VERIFY((sfinae_is_callable<T, T, V>(fma_test)));
    VERIFY((sfinae_is_callable<V, T, T>(fma_test)));
    VERIFY((sfinae_is_callable<T, V, T>(fma_test)));
    VERIFY((sfinae_is_callable<T, V, V>(fma_test)));
    VERIFY((sfinae_is_callable<V, T, V>(fma_test)));
    VERIFY((sfinae_is_callable<V, V, T>(fma_test)));
    VERIFY((sfinae_is_callable<int, int, V>(fma_test)));
    VERIFY((sfinae_is_callable<int, V, int>(fma_test)));
    VERIFY((sfinae_is_callable<V, T, int>(fma_test)));
    VERIFY((!sfinae_is_callable<V, T, bool>(fma_test)));
    VERIFY((!sfinae_is_callable<bool, V, V>(fma_test)));
  }

TEST_TYPES(V, ldexp_scalbn_scalbln_modf, real_test_types)
  {
    vir::test::setFuzzyness<float>(0);
    vir::test::setFuzzyness<double>(0);

    using T = typename V::value_type;

    // See https://sourceware.org/bugzilla/show_bug.cgi?id=18031
    const bool modf_is_broken = [] {
      volatile T x = T(5e20) / 7;
      T tmp;
      return std::fabs(std::modf(x, &tmp)) >= 1;
    }();
    if (modf_is_broken)
      __builtin_fprintf(stderr,
			"NOTE: Skipping modf because std::modf is broken.\n");

    test_values<V>(
      {
#ifdef __STDC_IEC_559__
	std::__quiet_NaN_v<T>,
	std::__infinity_v<T>,
	-std::__infinity_v<T>,
	-0.,
	std::__denorm_min_v<T>,
	std::__norm_min_v<T> / 3,
	-std::__denorm_min_v<T>,
	-std::__norm_min_v<T> / 3,
#endif
	+0.,
	+1.3,
	-1.3,
	2.1,
	-2.1,
	0.99,
	0.9,
	-0.9,
	-0.99,
	std::__norm_min_v<T>,
	std::__finite_max_v<T>,
	-std::__norm_min_v<T>,
	-std::__finite_max_v<T>},
      {10000},
      [](const V input) {
	for (int exp : {-10000, -100, -10, -1, 0, 1, 10, 100, 10000})
	  {
	    const auto totest = ldexp(input, exp);
	    using R = std::remove_const_t<decltype(totest)>;
	    auto&& expected = [&](const auto& v) -> const R {
	      R tmp = {};
	      using std::ldexp;
	      for (std::size_t i = 0; i < R::size(); ++i)
		{
		  tmp[i] = ldexp(v[i], exp);
		}
	      return tmp;
	    };
	    const R expect1 = expected(input);
	    COMPARE(isnan(totest), isnan(expect1))
	      << "ldexp(" << input << ", " << exp << ") = " << totest
	      << " != " << expect1;
	    FUZZY_COMPARE(ldexp(iif(isnan(expect1), 0, input), exp),
			  expected(iif(isnan(expect1), 0, input)))
	      << "\nclean = " << iif(isnan(expect1), 0, input);
	  }
      },
      [](const V input) {
	for (int exp : {-10000, -100, -10, -1, 0, 1, 10, 100, 10000})
	  {
	    const auto totest = scalbn(input, exp);
	    using R = std::remove_const_t<decltype(totest)>;
	    auto&& expected = [&](const auto& v) -> const R {
	      R tmp = {};
	      using std::scalbn;
	      for (std::size_t i = 0; i < R::size(); ++i)
		{
		  tmp[i] = scalbn(v[i], exp);
		}
	      return tmp;
	    };
	    const R expect1 = expected(input);
	    COMPARE(isnan(totest), isnan(expect1))
	      << "scalbn(" << input << ", " << exp << ") = " << totest
	      << " != " << expect1;
	    FUZZY_COMPARE(scalbn(iif(isnan(expect1), 0, input), exp),
			  expected(iif(isnan(expect1), 0, input)))
	      << "\nclean = " << iif(isnan(expect1), 0, input);
	  }
      },
      [](const V input) {
	for (long exp : {-10000, -100, -10, -1, 0, 1, 10, 100, 10000})
	  {
	    const auto totest = scalbln(input, exp);
	    using R = std::remove_const_t<decltype(totest)>;
	    auto&& expected = [&](const auto& v) -> const R {
	      R tmp = {};
	      using std::scalbln;
	      for (std::size_t i = 0; i < R::size(); ++i)
		{
		  tmp[i] = scalbln(v[i], exp);
		}
	      return tmp;
	    };
	    const R expect1 = expected(input);
	    COMPARE(isnan(totest), isnan(expect1))
	      << "scalbln(" << input << ", " << exp << ") = " << totest
	      << " != " << expect1;
	    FUZZY_COMPARE(scalbln(iif(isnan(expect1), 0, input), exp),
			  expected(iif(isnan(expect1), 0, input)))
	      << "\nclean = " << iif(isnan(expect1), 0, input);
	  }
      },
      [modf_is_broken](const V input) {
	if (modf_is_broken)
	  return;
	V integral = {};
	const V totest = modf(input, &integral);
	auto&& expected = [&](const auto& v) -> std::pair<const V, const V> {
	  std::pair<V, V> tmp = {};
	  using std::modf;
	  for (std::size_t i = 0; i < V::size(); ++i)
	    {
	      typename V::value_type tmp2;
	      tmp.first[i] = modf(v[i], &tmp2);
	      tmp.second[i] = tmp2;
	    }
	  return tmp;
	};
	const auto expect1 = expected(input);
#ifdef __STDC_IEC_559__
	COMPARE(isnan(totest), isnan(expect1.first))
	  << "modf(" << input << ", iptr) = " << totest << " != " << expect1;
	COMPARE(isnan(integral), isnan(expect1.second))
	  << "modf(" << input << ", iptr) = " << totest << " != " << expect1;
	COMPARE(isnan(totest), isnan(integral))
	  << "modf(" << input << ", iptr) = " << totest << " != " << expect1;
	const V clean = iif(isnan(totest), V(), input);
#else
	const V clean = iif(isnormal(input), input, V());
#endif
	const auto expect2 = expected(clean);
	COMPARE(modf(clean, &integral), expect2.first) << "\nclean = " << clean;
	COMPARE(integral, expect2.second);
      });
  }

TEST_TYPES(V, remqo, real_test_types)
  {
    vir::test::setFuzzyness<float>(0);
    vir::test::setFuzzyness<double>(0);

    using T = typename V::value_type;
    test_values_2arg<V>(
      {
#ifdef __STDC_IEC_559__
	std::__quiet_NaN_v<T>, std::__infinity_v<T>, -std::__infinity_v<T>,
	std::__denorm_min_v<T>, std::__norm_min_v<T> / 3, -0.,
#endif
	+0., std::__norm_min_v<T>, std::__finite_max_v<T>},
      {10000}, [](V a, V b) {

#ifndef __STDC_IEC_559__
	// without __STDC_IEC_559__, remquo(a, 0) is unspecified
	where(b == 0, b) = 1;
#endif
	using IV = std::experimental::fixed_size_simd<int, V::size()>;
	IV quo = {};
	const V totest = remquo(a, b, &quo);
	auto&& expected
	  = [&](const auto& v, const auto& w) -> std::pair<const V, const IV> {
	    std::pair<V, IV> tmp = {};
	    using std::remquo;
	    for (std::size_t i = 0; i < V::size(); ++i)
	      {
		int tmp2;
		tmp.first[i] = remquo(v[i], w[i], &tmp2);
		tmp.second[i] = tmp2;
	      }
	    return tmp;
	  };
	const auto expect1 = expected(a, b);
	COMPARE(isnan(totest), isnan(expect1.first))
	  << "remquo(" << a << ", " << b << ", quo) = " << totest
	  << " != " << expect1.first;
	const V clean_a = iif(isnan(totest), 0, a);
	const V clean_b = iif(isnan(totest), 1, b);
	const auto expect2 = expected(clean_a, clean_b);
	COMPARE(remquo(clean_a, clean_b, &quo), expect2.first)
	  << "\nclean_a/b = " << clean_a << ", " << clean_b;
	COMPARE(quo, expect2.second);
      });
  }
