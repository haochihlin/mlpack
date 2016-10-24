/**
 * @file drusilla_select_impl.hpp
 * @author Ryan Curtin
 *
 * Implementation of DrusillaSelect class methods.
 */
#ifndef MLPACK_METHODS_APPROX_KFN_DRUSILLA_SELECT_IMPL_HPP
#define MLPACK_METHODS_APPROX_KFN_DRUSILLA_SELECT_IMPL_HPP

// In case it hasn't been included yet.
#include "drusilla_select.hpp"

#include <queue>
#include <mlpack/methods/neighbor_search/neighbor_search_rules.hpp>
#include <mlpack/methods/neighbor_search/sort_policies/furthest_neighbor_sort.hpp>
#include <mlpack/core/tree/binary_space_tree.hpp>
#include <algorithm>

namespace mlpack {
namespace neighbor {

// Constructor.
template<typename MatType>
DrusillaSelect<MatType>::DrusillaSelect(const MatType& referenceSet,
                                        const size_t l,
                                        const size_t m) :
    candidateSet(referenceSet.n_rows, l * m),
    l(l),
    m(m)
{
  if (l == 0)
    throw std::invalid_argument("DrusillaSelect::DrusillaSelect(): invalid "
        "value of l; must be greater than 0!");
  else if (m == 0)
    throw std::invalid_argument("DrusillaSelect::DrusillaSelect(): invalid "
        "value of m; must be greater than 0!");

  Train(referenceSet, l, m);
}

// Constructor with no training.
template<typename MatType>
DrusillaSelect<MatType>::DrusillaSelect(const size_t l, const size_t m) :
    l(l),
    m(m)
{
  if (l == 0)
    throw std::invalid_argument("DrusillaSelect::DrusillaSelect(): invalid "
        "value of l; must be greater than 0!");
  else if (m == 0)
    throw std::invalid_argument("DrusillaSelect::DrusillaSelect(): invalid "
        "value of m; must be greater than 0!");
}

// Train the model.
template<typename MatType>
void DrusillaSelect<MatType>::Train(
    const MatType& referenceSet,
    const size_t lIn,
    const size_t mIn)
{
  // Did the user specify a new size?  If so, use it.
  if (lIn > 0)
    l = lIn;
  if (mIn > 0)
    m = mIn;

  if ((l * m) > referenceSet.n_cols)
    throw std::invalid_argument("DrusillaSelect::Train(): l and m are too "
        "large!  Choose smaller values.  l*m must be smaller than the number "
        "of points in the dataset.");

  arma::vec dataMean = arma::mean(referenceSet, 1);
  arma::vec norms(referenceSet.n_cols);

  arma::mat refCopy = referenceSet.each_col() - dataMean;
  for (size_t i = 0; i < refCopy.n_cols; ++i)
    norms[i] = arma::norm(refCopy.col(i) - dataMean);

  // Find the top m points for each of the l projections...
  for (size_t i = 0; i < l; ++i)
  {
    // Pick best index.
    arma::uword maxIndex;
    norms.max(maxIndex);

    arma::vec line = refCopy.col(maxIndex) / arma::norm(refCopy.col(maxIndex));
    const size_t n_nonzero = (size_t) arma::sum(norms > 0);

    // Calculate distortion and offset.
    arma::vec distortions(referenceSet.n_cols);
    arma::vec offsets(referenceSet.n_cols);
    for (size_t j = 0; j < referenceSet.n_cols; ++j)
    {
      if (norms[j] > 0.0)
      {
        offsets[j] = arma::dot(refCopy.col(j), line);
        distortions[j] = arma::norm(refCopy.col(j) - offsets[j] *
            line);
      }
      else
      {
        offsets[j] = 0.0;
        distortions[j] = 0.0;
      }
    }
    arma::vec sums = arma::abs(offsets) - arma::abs(distortions);
    arma::uvec sortedSums = arma::sort_index(sums, "descend");

    arma::vec bestSums(m);
    arma::Col<size_t> bestIndices(m);
    bestSums.fill(-DBL_MAX);

    // Find the top m elements using a priority queue.
    typedef std::pair<double, size_t> Candidate;
    struct CandidateCmp
    {
      bool operator()(const Candidate& c1, const Candidate& c2)
      {
        return c2.first > c1.first;
      }
    };

    std::vector<Candidate> clist(m, std::make_pair(size_t(-1), double(0.0)));
    std::priority_queue<Candidate, std::vector<Candidate>, CandidateCmp>
        pq(CandidateCmp(), std::move(clist));

    for (size_t j = 0; j < sums.n_elem; ++j)
    {
      Candidate c = std::make_pair(sums[j], j);
      if (CandidateCmp()(c, pq.top()))
      {
        pq.pop();
        pq.push(c);
      }
    }

    // Take the top m elements for this table.
    for (size_t j = 0; j < m; ++j)
    {
      const size_t index = pq.top().second;
      pq.pop();
      candidateSet.col(i * m + j) = referenceSet.col(index);

      // Mark the norm as 0 so we don't see this point again.
      norms[index] = 0.0;
    }

    // Calculate angles from the current projection.  Anything close enough,
    // mark the norm as 0.
    arma::vec farPoints = arma::conv_to<arma::vec>::from(
        arma::atan(distortions / arma::abs(offsets)) >= (M_PI / 8.0));
    norms %= farPoints;
  }
}

// Search.
template<typename MatType>
void DrusillaSelect<MatType>::Search(const MatType& querySet,
                                     const size_t k,
                                     arma::Mat<size_t>& neighbors,
                                     arma::mat& distances)
{
  if (candidateSet.n_cols == 0)
    throw std::runtime_error("DrusillaSelect::Search(): candidate set not "
        "initialized!  Call Train() first.");

  if (k > (l * m))
    throw std::invalid_argument("DrusillaSelect::Search(): requested k is "
        "greater than number of points in candidate set!  Increase l or m.");

  // We'll use the NeighborSearchRules class to perform our brute-force search.
  // Note that we aren't using trees for our search, so we can use 'int' as a
  // TreeType.
  metric::EuclideanDistance metric;
  NeighborSearchRules<FurthestNeighborSort, metric::EuclideanDistance,
      tree::KDTree<metric::EuclideanDistance, tree::EmptyStatistic, arma::mat>>
      rules(querySet, candidateSet, k, metric, 0, false);

  neighbors.set_size(k, querySet.n_cols);
  neighbors.fill(size_t() - 1);
  distances.zeros(k, querySet.n_cols);

  for (size_t q = 0; q < querySet.n_cols; ++q)
    for (size_t r = 0; r < candidateSet.n_cols; ++r)
      rules.BaseCase(q, r);

  // Map the neighbors back to their original indices in the reference set.
  for (size_t i = 0; i < neighbors.n_elem; ++i)
    neighbors[i] = candidateIndices[neighbors[i]];
}

//! Serialize the model.
template<typename MatType>
template<typename Archive>
void DrusillaSelect<MatType>::Serialize(Archive& ar,
                                        const unsigned int /* version */)
{
  using data::CreateNVP;

  ar & CreateNVP(candidateSet, "candidateSet");
  ar & CreateNVP(candidateIndices, "candidateIndices");
  ar & CreateNVP(l, "l");
  ar & CreateNVP(m, "m");
}

} // namespace neighbor
} // namespace mlpack

#endif
