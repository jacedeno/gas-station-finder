// Host-side unit tests for the geo math. Run: `pio test -e native`.
#include <unity.h>

#include <geo/geo.h>

// 1 degree of longitude at the equator is ~111.19 km (great-circle).
void test_haversine_equator_one_degree() {
  double d = geo::haversineMeters(0.0, 0.0, 0.0, 1.0);
  TEST_ASSERT_FLOAT_WITHIN(200.0, 111195.0, d);
}

void test_haversine_zero() {
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0,
                           geo::haversineMeters(28.5, -81.4, 28.5, -81.4));
}

void test_bearing_due_north() {
  TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, geo::bearingDeg(0.0, 0.0, 1.0, 0.0));
}

void test_bearing_due_east() {
  TEST_ASSERT_FLOAT_WITHIN(0.01, 90.0, geo::bearingDeg(0.0, 0.0, 0.0, 1.0));
}

void test_angular_diff_wraps() {
  TEST_ASSERT_FLOAT_WITHIN(0.01, 20.0, geo::angularDiffDeg(350.0, 10.0));
  TEST_ASSERT_FLOAT_WITHIN(0.01, 180.0, geo::angularDiffDeg(0.0, 180.0));
  TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, geo::angularDiffDeg(45.0, 45.0));
}

void test_is_ahead() {
  // Within +/-70 deg of course 0.
  TEST_ASSERT_TRUE(geo::isAhead(30.0, 0.0, 70.0));
  TEST_ASSERT_TRUE(geo::isAhead(330.0, 0.0, 70.0));  // -30 deg, wraps
  TEST_ASSERT_FALSE(geo::isAhead(100.0, 0.0, 70.0));  // behind-ish
  TEST_ASSERT_FALSE(geo::isAhead(180.0, 0.0, 70.0));  // straight behind
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_haversine_equator_one_degree);
  RUN_TEST(test_haversine_zero);
  RUN_TEST(test_bearing_due_north);
  RUN_TEST(test_bearing_due_east);
  RUN_TEST(test_angular_diff_wraps);
  RUN_TEST(test_is_ahead);
  return UNITY_END();
}
