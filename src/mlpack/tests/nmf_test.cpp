/**
 * @file nmf_test.cpp
 * @author Mohan Rajendran
 *
 * Test file for NMF class.
 */
#include <mlpack/core.hpp>
#include <mlpack/methods/amf/amf.hpp>
#include <mlpack/methods/amf/init_rules/random_acol_init.hpp>
#include <mlpack/methods/amf/update_rules/nmf_mult_div.hpp>
#include <mlpack/methods/amf/update_rules/nmf_als.hpp>

#include <boost/test/unit_test.hpp>
#include "old_boost_test_definitions.hpp"

BOOST_AUTO_TEST_SUITE(NMFTest);

using namespace std;
using namespace arma;
using namespace mlpack;
using namespace mlpack::amf;

/**
 * Check the if the product of the calculated factorization is close to the
 * input matrix. Default case.
 */
BOOST_AUTO_TEST_CASE(NMFDefaultTest)
{
  mat w = randu<mat>(20, 16);
  mat h = randu<mat>(16, 20);
  mat v = w * h;
  size_t r = 16;

  AMF<> nmf;
  nmf.Apply(v, r, w, h);

  mat wh = w * h;

  for (size_t row = 0; row < 20; row++)
    for (size_t col = 0; col < 20; col++)
      BOOST_REQUIRE_CLOSE(v(row, col), wh(row, col), 10.0);
}

/**
 * Check the if the product of the calculated factorization is close to the
 * input matrix. Random Acol initialization distance minimization update.
 */
BOOST_AUTO_TEST_CASE(NMFAcolDistTest)
{
  mat w = randu<mat>(20, 16);
  mat h = randu<mat>(16, 20);
  mat v = w * h;
  size_t r = 16;

  AMF<RandomAcolInitialization<> > nmf;
  nmf.Apply(v, r, w, h);

  mat wh = w * h;

  for (size_t row = 0; row < 20; row++)
    for (size_t col = 0; col < 20; col++)
      BOOST_REQUIRE_CLOSE(v(row, col), wh(row, col), 10.0);
}

/**
 * Check the if the product of the calculated factorization is close to the
 * input matrix. Random initialization divergence minimization update.
 */
BOOST_AUTO_TEST_CASE(NMFRandomDivTest)
{
  mat w = randu<mat>(20, 16);
  mat h = randu<mat>(16, 20);
  mat v = w * h;
  size_t r = 16;

  AMF<RandomInitialization, NMFMultiplicativeDivergenceUpdate> nmf;
  nmf.Apply(v, r, w, h);

  mat wh = w * h;

  for (size_t row = 0; row < 20; row++)
    for (size_t col = 0; col < 20; col++)
      BOOST_REQUIRE_CLOSE(v(row, col), wh(row, col), 10.0);
}

/**
 * Check that the product of the calculated factorization is close to the
 * input matrix.  This uses the random initialization and alternating least
 * squares update rule.
 */
BOOST_AUTO_TEST_CASE(NMFALSTest)
{
  mat w = randu<mat>(20, 16);
  mat h = randu<mat>(16, 20);
  mat v = w * h;
  size_t r = 16;

  AMF<RandomInitialization, NMFALSUpdate> nmf(50000, 1e-15);
  nmf.Apply(v, r, w, h);

  const mat wh = w * h;

  const double error = arma::accu(arma::abs(v - wh)) / arma::accu(v);

  // Most runs seem to produce error between 0.01 and 0.03.
  // This is a randomized test, so we don't really have guarantees.
  BOOST_REQUIRE_SMALL(error, 0.04);
}

/**
 * Check the if the product of the calculated factorization is close to the
 * input matrix, with a sparse input matrix.. Default case.
 */
BOOST_AUTO_TEST_CASE(SparseNMFDefaultTest)
{
  mat w, h;
  sp_mat v;
  v.sprandu(20, 20, 0.2);
  mat dv(v); // Make a dense copy.
  mat dw, dh;
  size_t r = 18;

  // It seems to hit the iteration limit first.
  AMF<> nmf(10000, 1e-20);
  mlpack::math::RandomSeed(1000); // Set random seed so results are the same.
  nmf.Apply(v, r, w, h);
  mlpack::math::RandomSeed(1000);
  nmf.Apply(dv, r, dw, dh);

  // Make sure the results are about equal for the W and H matrices.
  for (size_t i = 0; i < w.n_elem; ++i)
  {
    if (w(i) == 0.0)
      BOOST_REQUIRE_SMALL(dw(i), 1e-15);
    else
      BOOST_REQUIRE_CLOSE(w(i), dw(i), 1e-5);
  }

  for (size_t i = 0; i < h.n_elem; ++i)
  {
    if (h(i) == 0.0)
      BOOST_REQUIRE_SMALL(dh(i), 1e-15);
    else
      BOOST_REQUIRE_CLOSE(h(i), dh(i), 1e-5);
  }
}

/**
 * Check the if the product of the calculated factorization is close to the
 * input matrix, with a sparse input matrix. Random Acol initialization,
 * distance minimization update.
 */
BOOST_AUTO_TEST_CASE(SparseNMFAcolDistTest)
{
  mat w, h;
  sp_mat v;
  v.sprandu(20, 20, 0.3);
  mat dv(v); // Make a dense copy.
  mat dw, dh;
  size_t r = 16;

  AMF<RandomAcolInitialization<> > nmf;
  mlpack::math::RandomSeed(1000); // Set random seed so results are the same.
  nmf.Apply(v, r, w, h);
  mlpack::math::RandomSeed(1000);
  nmf.Apply(dv, r, dw, dh);

  // Make sure the results are about equal for the W and H matrices.
  for (size_t i = 0; i < w.n_elem; ++i)
  {
    if (w(i) == 0.0)
      BOOST_REQUIRE_SMALL(dw(i), 1e-15);
    else
      BOOST_REQUIRE_CLOSE(w(i), dw(i), 1e-5);
  }

  for (size_t i = 0; i < h.n_elem; ++i)
  {
    if (h(i) == 0.0)
      BOOST_REQUIRE_SMALL(dh(i), 1e-15);
    else
      BOOST_REQUIRE_CLOSE(h(i), dh(i), 1e-5);
  }
}

/**
 * Check the if the product of the calculated factorization is close to the
 * input matrix, with a sparse input matrix. Random initialization, divergence
 * minimization update.
 */
BOOST_AUTO_TEST_CASE(SparseNMFRandomDivTest)
{
  mat w, h;
  sp_mat v;
  v.sprandu(20, 20, 0.3);
  mat dv(v); // Make a dense copy.
  mat dw, dh;
  size_t r = 16;

  AMF<RandomInitialization, NMFMultiplicativeDivergenceUpdate> nmf;
  mlpack::math::RandomSeed(10); // Set random seed so the results are the same.
  nmf.Apply(v, r, w, h);
  mlpack::math::RandomSeed(10);
  nmf.Apply(dv, r, dw, dh);

  // Make sure the results are about equal for the W and H matrices.
  for (size_t i = 0; i < w.n_elem; ++i)
  {
    if (w(i) == 0.0)
      BOOST_REQUIRE_SMALL(dw(i), 1e-15);
    else
      BOOST_REQUIRE_CLOSE(w(i), dw(i), 1e-5);
  }

  for (size_t i = 0; i < h.n_elem; ++i)
  {
    if (h(i) == 0.0)
      BOOST_REQUIRE_SMALL(dh(i), 1e-15);
    else
      BOOST_REQUIRE_CLOSE(h(i), dh(i), 1e-5);
  }
}

/**
 * Check that the product of the calculated factorization is close to the
 * input matrix, with a sparse input matrix.  This uses the random
 * initialization and alternating least squares update rule.
 */
BOOST_AUTO_TEST_CASE(SparseNMFALSTest)
{
  mat w, h;
  sp_mat v;
  v.sprandu(10, 10, 0.3);
  mat dv(v); // Make a dense copy.
  mat dw, dh;
  size_t r = 8;

  AMF<RandomInitialization, NMFALSUpdate> nmf;
  mlpack::math::RandomSeed(40);
  nmf.Apply(v, r, w, h);
  mlpack::math::RandomSeed(40);
  nmf.Apply(dv, r, dw, dh);

  // Make sure the results are about equal for the W and H matrices.
  for (size_t i = 0; i < w.n_elem; ++i)
  {
    if (w(i) == 0.0)
      BOOST_REQUIRE_SMALL(dw(i), 1e-15);
    else
      BOOST_REQUIRE_CLOSE(w(i), dw(i), 1e-5);
  }

  for (size_t i = 0; i < h.n_elem; ++i)
  {
    if (h(i) == 0.0)
      BOOST_REQUIRE_SMALL(dh(i), 1e-15);
    else
      BOOST_REQUIRE_CLOSE(h(i), dh(i), 1e-5);
  }
}

BOOST_AUTO_TEST_SUITE_END();
