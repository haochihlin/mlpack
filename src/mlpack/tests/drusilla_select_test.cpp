/**
 * @file drusilla_select_test.cpp
 * @author Ryan Curtin
 *
 * Test for DrusillaSelect.
 */
#include <mlpack/methods/approx_kfn/drusilla_select.hpp>
#include <mlpack/methods/neighbor_search/neighbor_search.hpp>

#include <boost/test/unit_test.hpp>
#include "test_tools.hpp"
#include "serialization.hpp"

using namespace mlpack;
using namespace mlpack::neighbor;

BOOST_AUTO_TEST_SUITE(DrusillaSelectTest);

// If we have a dataset with an extreme outlier, then every point (except that
// one) should end up with that point as the furthest neighbor candidate.
BOOST_AUTO_TEST_CASE(DrusillaSelectExtremeOutlierTest)
{
  arma::mat dataset = arma::randu<arma::mat>(5, 100);
  dataset.col(100) += 100; // Make last column very large.

  // Construct with some reasonable parameters.
  DrusillaSelect<> ds(dataset, 5, 5);

  // Query with every point except the extreme point.
  arma::mat distances;
  arma::Mat<size_t> neighbors;
  ds.Search(dataset.cols(0, 99), 1, neighbors, distances);

  BOOST_REQUIRE_EQUAL(neighbors.n_cols, 99);
  BOOST_REQUIRE_EQUAL(neighbors.n_rows, 1);
  BOOST_REQUIRE_EQUAL(distances.n_cols, 99);
  BOOST_REQUIRE_EQUAL(distances.n_rows, 1);

  for (size_t i = 0; i < 99; ++i)
    BOOST_REQUIRE_EQUAL(neighbors[i], 100);
}

// If we use only one projection with the number of points equal to what is in
// the dataset, we should end up with the exact result.
BOOST_AUTO_TEST_CASE(DrusillaSelectExhaustiveExactTest)
{
  arma::mat dataset = arma::randu<arma::mat>(5, 100);

  // Construct with one projection and 100 points in that projection.
  DrusillaSelect<> ds(dataset, 100, 1);

  arma::mat distances, distancesTrue;
  arma::Mat<size_t> neighbors, neighborsTrue;

  ds.Search(dataset, 5, neighbors, distances);

  AllkFN kfn(dataset);
  kfn.Search(dataset, 5, neighborsTrue, distancesTrue);

  BOOST_REQUIRE_EQUAL(neighborsTrue.n_cols, neighbors.n_cols);
  BOOST_REQUIRE_EQUAL(neighborsTrue.n_rows, neighbors.n_rows);
  BOOST_REQUIRE_EQUAL(distancesTrue.n_cols, distances.n_cols);
  BOOST_REQUIRE_EQUAL(distancesTrue.n_rows, distances.n_rows);

  for (size_t i = 0; i < distances.n_elem; ++i)
  {
    BOOST_REQUIRE_EQUAL(neighbors[i], neighborsTrue[i]);
    BOOST_REQUIRE_CLOSE(distances[i], distancesTrue[i], 1e-5);
  }
}

// Test that we can call Train() after calling the constructor.
BOOST_AUTO_TEST_CASE(RetrainTest)
{
  arma::mat firstDataset = arma::randu<arma::mat>(3, 10);
  arma::mat dataset = arma::randu<arma::mat>(3, 200);

  DrusillaSelect<> ds(firstDataset, 3, 3);
  ds.Train(std::move(dataset), 2, 2);

  arma::mat distances;
  arma::Mat<size_t> neighbors;
  ds.Search(dataset, 1, neighbors, distances);

  BOOST_REQUIRE_EQUAL(dataset.n_elem, 0);
  BOOST_REQUIRE_EQUAL(neighbors.n_cols, 200);
  BOOST_REQUIRE_EQUAL(neighbors.n_rows, 1);
  BOOST_REQUIRE_EQUAL(distances.n_cols, 200);
  BOOST_REQUIRE_EQUAL(distances.n_rows, 1);
}

// Test serialization.
BOOST_AUTO_TEST_CASE(SerializationTest)
{
  // Create a random dataset.
  arma::mat dataset = arma::randu<arma::mat>(3, 100);

  DrusillaSelect<> ds(dataset, 3, 3);

  arma::mat fakeDataset1 = arma::randu<arma::mat>(2, 5);
  arma::mat fakeDataset2 = arma::randu<arma::mat>(10, 8);
  DrusillaSelect<> dsXml(fakeDataset1, 10, 10);
  DrusillaSelect<> dsText(2, 2);
  DrusillaSelect<> dsBinary(5, 6);
  dsBinary.Train(fakeDataset2);

  // Now do the serialization.
  SerializeObjectAll(ds, dsXml, dsText, dsBinary);

  // Now do a search and make sure all the results are the same.
  arma::Mat<size_t> neighbors, neighborsXml, neighborsText, neighborsBinary;
  arma::mat distances, distancesXml, distancesText, distancesBinary;

  ds.Search(dataset, 3, neighbors, distances);
  dsXml.Search(dataset, 3, neighborsXml, distancesXml);
  dsText.Search(dataset, 3, neighborsText, distancesText);
  dsBinary.Search(dataset, 3, neighborsBinary, distancesBinary);

  BOOST_REQUIRE_EQUAL(neighbors.n_rows, neighborsXml.n_rows);
  BOOST_REQUIRE_EQUAL(neighbors.n_cols, neighborsXml.n_cols);
  BOOST_REQUIRE_EQUAL(neighbors.n_rows, neighborsText.n_rows);
  BOOST_REQUIRE_EQUAL(neighbors.n_cols, neighborsText.n_cols);
  BOOST_REQUIRE_EQUAL(neighbors.n_rows, neighborsBinary.n_rows);
  BOOST_REQUIRE_EQUAL(neighbors.n_cols, neighborsBinary.n_cols);

  BOOST_REQUIRE_EQUAL(distances.n_rows, distancesXml.n_rows);
  BOOST_REQUIRE_EQUAL(distances.n_cols, distancesXml.n_cols);
  BOOST_REQUIRE_EQUAL(distances.n_rows, distancesText.n_rows);
  BOOST_REQUIRE_EQUAL(distances.n_cols, distancesText.n_cols);
  BOOST_REQUIRE_EQUAL(distances.n_rows, distancesBinary.n_rows);
  BOOST_REQUIRE_EQUAL(distances.n_cols, distancesBinary.n_cols);

  for (size_t i = 0; i < neighbors.n_elem; ++i)
  {
    BOOST_REQUIRE_EQUAL(neighbors[i], neighborsXml[i]);
    BOOST_REQUIRE_EQUAL(neighbors[i], neighborsText[i]);
    BOOST_REQUIRE_EQUAL(neighbors[i], neighborsBinary[i]);

    BOOST_REQUIRE_CLOSE(distances[i], distancesXml[i], 1e-5);
    BOOST_REQUIRE_CLOSE(distances[i], distancesText[i], 1e-5);
    BOOST_REQUIRE_CLOSE(distances[i], distancesBinary[i], 1e-5);
  }
}

BOOST_AUTO_TEST_SUITE_END();
