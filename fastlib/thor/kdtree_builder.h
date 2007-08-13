/**
 * @file kdtree_builder.h
 *
 * A kd-tree builder suitable for THOR's parallelism.
 *
 * At the moment, the treebuilder isn't very parallel.
 */

#ifndef THOR_KDTREE_BUILDER_H
#define THOR_KDTREE_BUILDER_H

#include "thortree.h"
#include "cachearray.h"

#include "file/textfile.h"
#include "tree/bounds.h"
#include "fx/fx.h"
#include "base/common.h"

/**
 * A generalized partition function for cache arrays.
 */
template<typename PartitionCondition, typename PointCache, typename Bound>
index_t Partition(
    PartitionCondition splitcond,
    index_t begin, index_t count,
    PointCache* points,
    Bound* left_bound, Bound* right_bound) {
  index_t left_i = begin;
  index_t right_i = begin + count - 1;

  /* At any point:
   *   every thing that strictly precedes left_i is correct
   *   every thing that strictly succeeds right_i is correct
   */
  for (;;) {
    for (;;) {
      if (unlikely(left_i > right_i)) return left_i;
      CacheRead<typename PointCache::Element> left_v(points, left_i);
      if (!splitcond.is_left(left_v->vec())) {
        *right_bound |= left_v->vec();
        break;
      }
      *left_bound |= left_v->vec();
      left_i++;
    }

    for (;;) {
      if (unlikely(left_i > right_i)) return left_i;
      CacheRead<typename PointCache::Element> right_v(points, right_i);
      if (splitcond.is_left(right_v->vec())) {
        *left_bound |= right_v->vec();
        break;
      }
      *right_bound |= right_v->vec();
      right_i--;
    }

    points->Swap(left_i, right_i);

    DEBUG_ASSERT(left_i <= right_i);
    right_i--;
  }

  abort();
}

/**
 * Single-threaded kd-tree builder.
 *
 * Rearranges points in place and attempts to take advantage of the block
 * structure.
 *
 * The algorithm uses a combination of midpoint and median splits.
 * At the higher levels of the tree, a median-like split is done such that
 * the split falls on the block boundary (or otherwise specified chunk_size)
 * that is closest to the middle index.  Once the number of points
 * considered is smaller than the chunk size, midpoint splits are done.
 * The median splits simplify load balancing and allow more efficient
 * storage of data, and actually help the dual-tree algorithm in the
 * initial few layers -- however, the midpoint splits help to separate
 * outliers from the rest of the data.  Leaves are created once the number
 * of points is at most leaf_size.
 */
template<typename TPoint, typename TNode, typename TParam>
class KdTreeHybridBuilder {
 public:
  typedef TNode Node;
  typedef TPoint Point;
  typedef typename TNode::Bound Bound;
  typedef TParam Param;
  typedef ThorTreeDecomposition<Node> TreeDecomposition;
  typedef typename TreeDecomposition::DecompNode DecompNode;

 private:
  struct HrectPartitionCondition {
    int dimension;
    double value;

    HrectPartitionCondition(int dimension_in, double value_in)
      : dimension(dimension_in)
      , value(value_in) {}

    bool is_left(const Vector& vector) const {
      return vector.get(dimension) < value;
    }
  };

 private:
  const Param* param_;
  CacheArray<Point> points_;
  CacheArray<Node> nodes_;
  index_t leaf_size_;
  index_t chunk_size_;
  index_t n_points_;

 public:
  /**
   * Builds a kd-tree.
   *
   * See class comments.
   *
   * @param module module for tuning parameters: leaf_size (maximum
   *        number of points per leaf), and chunk_size (rounding granularity
   *        for median splits)
   * @param param parameters needed by the bound or other structures
   * @param begin_index the first index that I'm building
   * @param end_index one beyond the last index
   * @param points_inout the points, to be reordered
   * @param nodes_create the nodes, which will be allocated one by one
   */
  void Doit(
      struct datanode* module, const Param* param_in_,
      index_t begin_index, index_t end_index,
      DistributedCache* points_inout, DistributedCache* nodes_create,
      TreeDecomposition* decomposition);
 
 private:
  /** Determines the bounding box for a range of points. */
  void FindBoundingBox_(index_t begin, index_t count, Bound* bound);

  /** Builds a specific node in the tree. */
  index_t Build_(index_t begin_col, index_t end_col,
      int begin_rank, int end_rank,
      const Bound& bound, Node* parent, DecompNode** decomp_pp);

  /** Splits a node in the tree. */
  void Split_(Node* node, int begin_rank, int end_rank, int split_dim,
      DecompNode** left_decomp_pp, DecompNode** right_decomp_pp);
};

template<typename TPoint, typename TNode, typename TParam>
void KdTreeHybridBuilder<TPoint, TNode, TParam>::Doit(
    struct datanode* module, const Param* param_in,
    index_t begin_index, index_t end_index,
    DistributedCache* points_inout, DistributedCache* nodes_create,
    TreeDecomposition* decomposition) {
  param_ = param_in;
  n_points_ = end_index - begin_index;

  points_.Init(points_inout, BlockDevice::M_MODIFY);
  nodes_.Init(nodes_create, BlockDevice::M_CREATE);

  index_t dimension;

  {
    CacheRead<Point> first_point(&points_, points_.begin_index());
    dimension = first_point->vec().length();
  }

  leaf_size_ = fx_param_int(module, "leaf_size", 32);
  chunk_size_ = points_.n_block_elems();

  fx_timer_start(module, "tree_build");
  DecompNode* decomp_root;
  Bound bound;
  bound.Init(dimension);
  FindBoundingBox_(node->begin(), node->end(), &bound);
  Build_(begin_index, end_index, 0, rpc::rank(), bound, NULL, &decomp_root);
  decomposition->Init(decomp_root);
  fx_timer_stop(module, "tree_build");
}

template<typename TPoint, typename TNode, typename TParam>
void KdTreeHybridBuilder<TPoint, TNode, TParam>::FindBoundingBox_(
    index_t begin, index_t count, Bound* bound) {
  CacheReadIter<Point> point(&points_, begin);
  for (index_t i = count; i--; point.Next()) {
    *bound |= point->vec();
  }
}

template<typename TPoint, typename TNode, typename TParam>
void KdTreeHybridBuilder<TPoint, TNode, TParam>::Build_(
    index_t begin_col, index_t end_col,
    int begin_rank, int end_rank, const Bound& bound,
    Node* parent, DecompNode** decomp_pp) {
  index_t node_i = nodes_.AllocD(begin_rank);
  Node* node = nodes_.StartWrite(node_i);
  bool leaf = (node->count() <= leaf_size_);
  DecompNode* left_decomp = NULL;
  DecompNode* right_decomp = NULL;

  node->set_range(begin_col, end_col - begin_col);
  node->bound().Reset();
  node->bound() |= bound;

  if (!leaf) {
    index_t split_dim = BIG_BAD_NUMBER;
    double max_width = -1;

    // Short loop to find widest dimension
    for (index_t d = 0; d < dim_; d++) {
      double w = node->bound().get(d).width();

      if (unlikely(w > max_width)) {
        max_width = w;
        split_dim = d;
      }
    }

    // even if the max width is zero, we still* must* split it!
    Split_(node, begin_rank, end_rank, split_dim,
        &left_decomp, &right_decomp);
  } else {
    node->set_leaf();
    // ensure leaves don't straddle block boundaries
    DEBUG_SAME_INT(node->begin() / points_.n_block_elems(),
        (node->end() - 1) / points_.n_block_elems());
    for (index_t i = node->begin(); i < node->end(); i++) {
      CacheRead<Point> point(&points_, i);
      node->stat().Accumulate(*param_, *point);
    }
  }

  if (parent != NULL) {
    // accumulate self to parent's statistics
    parent->stat().Accumulate(*param_,
        node->stat(), node->bound(), node->count());
  }

  node->stat().Postprocess(*param_, node->bound(), node->count());

  if (decomp_pp) {
    *decomp_pp = new DecompNode(
        typename TreeDecomposition::Info(begin_rank, end_rank),
        &nodes_, node_i, nodes_.end_index());
    DEBUG_ASSERT((left_decomp == NULL) == (right_decomp == NULL));
    if (left_decomp != NULL) {
      (*decomp_pp)->set_child(0, left_decomp);
      (*decomp_pp)->set_child(1, right_decomp);
    }
  }

  nodes_.StopWrite(node_i);

  return decomp;
}

template<typename TPoint, typename TNode, typename TParam>
void KdTreeHybridBuilder<TPoint, TNode, TParam>::Split_(
    Node* node, int begin_rank, int end_rank, int split_dim,
    DecompNode** left_decomp_pp, DecompNode *right_decomp_pp) {
  index_t split_col;
  index_t begin_col = node->begin();
  index_t end_col = node->end();
  int split_rank = (begin_rank + end_rank) / 2;
  double split_val;
  DRange current_range = node->bound().get(split_dim);
  typename Node::Bound final_left_bound;
  typename Node::Bound final_right_bound;

  final_left_bound.Init(dim_);
  final_right_bound.Init(dim_);

  if ((node->begin() & points_.n_block_elems_mask()) == 0
      && (!parent || parent->begin() != node->begin())) {
    // We got one block of points!  Let's give away ownership.
    points_.cache()->GiveOwnership(
        points_.Blockid(node->begin()), begin_rank);
    // This is also a convenient time to display status.
    percent_indicator("tree built", node->end(), n_points_);
  }

  if (node->count() <= chunk_size_) {
    split_val = current_range.mid();
    if (current_range.wid() == 0) {
      // All points are equal.  As a point of diligence, we still divide it,
      // into two overlapping nodes.
      split_col = (node->begin() + node->end()) / 2;
    } else {
      // perform a midpoint split
      split_col = Partition(
          HrectPartitionCondition(split_dim, split_val),
          begin_col, end_col - begin_col,
          &points_, &final_left_bound, &final_right_bound);
    }
  } else {
    index_t goal_col;
    typename Node::Bound left_bound;
    typename Node::Bound right_bound;
    left_bound.Init(dim_);
    right_bound.Init(dim_);

    if (end_rank <= begin_rank + 1) {
      // All points will go on the same machine, so do median split.
      goal_col = (begin_col + end_col) / 2;
    } else {
      // We're distributing these between machines.  Let's make sure
      // we give roughly even work to the machines.  What we do is
      // pretend the points are distributed as equally as possible, by
      // using the global number of machines and points, to avoid errors
      // introduced by doing this split computation recursively.
      goal_col = (uint64(split_rank) * 2 * n_points_ + rpc::n_peers())
          / rpc::n_peers() / 2;
    }

    // Round the goal to the nearest block.
    goal_col = (goal_col + chunk_size_ / 2) / chunk_size_ * chunk_size_;

    for (;;) {
      // use linear interpolation to guess the value to split on.
      // this typically leads to convergence rather quickly.
      split_val = current_range.interpolate(
          (goal_col - begin_col) / double(end_col - begin_col));

      left_bound.Reset();
      right_bound.Reset();
      split_col = Partition(
          HrectPartitionCondition(split_dim, split_val),
          begin_col, end_col - begin_col,
          &points_, &left_bound, &right_bound);

      if (split_col == goal_col) {
        final_left_bound |= left_bound;
        final_right_bound |= right_bound;
        break;
      } else if (split_col < goal_col) {
        final_left_bound |= left_bound;
        current_range = right_bound.get(split_dim);
        if (current_range.width() == 0) {
          // right_bound straddles the boundary, force it to break up
          final_right_bound |= right_bound;
          final_left_bound |= right_bound;
          split_col = goal_col;
          break;
        }
        begin_col = split_col;
      } else if (split_col > goal_col) {
        final_right_bound |= right_bound;
        current_range = left_bound.get(split_dim);
        if (current_range.width() == 0) {
          // left_bound straddles the boundary, force it to break up
          final_left_bound |= left_bound;
          final_right_bound |= left_bound;
          split_col = goal_col;
          break;
        }
        end_col = split_col;
      }
    }
  }

  if (end_rank - begin_rank <= 1) {
    // I'm only one machine, don't need to expand children.
    left_decomp_pp = right_decomp_pp = NULL;
  }

  node->set_child(0, Build_(begin_col, split_col,
      begin_rank, split_rank, final_left_bound, node, left_decomp_pp));
  node->set_child(1, Build_(split_col, end_col,
      split_rank, end_rank, final_right_bound, node, right_decomp_pp));
}

#endif
