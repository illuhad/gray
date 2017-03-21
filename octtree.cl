#ifndef OCTREE_CL
#define OCTREE_CL

#include "math.cl"

typedef int octtree_bucket_id;
typedef uchar16 octtree_node_id;
typedef int4 octree_value_type;


typedef struct
{
  octtree_node_id key;
  octree_value_type data;
  unsigned char populated_subnodes;
  octtree_bucket_id next;
} octtree_cell_data;

#define MAX_LEVEL 40

typedef struct {
  int num_buckets;
  __global octtree_node_id* keys;
  __global octtree_value_type* data;
  __global octtree_bucket_id* next;
} octtree_data_ctx;

/// Generates a bit pattern of \c width ones, starting from the
/// lowest bit
#define GENERATE_WIDTH_MASK(width) ((1 << width) - 1)
/// Returns the bit sequence contained in \c x that starts with
/// the lowest bit pos. \c width_mask should be a sequence of ones,
/// denoting the length of the bit pattern that is to be extracted.
#define EXTRACT_BITS(x, pos, width_mask) ((x >> pos) & width_mask)

int octtree_node_id_get_level(octtree_node_id ctx)
{
  return ctx.x & 0x3f; // 63 to mask the lowest 6 bits
}

uchar8 octtree_node_id_get_bit_triplets(unsigned char* byte_triplets, int triplet_index)
{
  const int width_mask3bit = 7;

  uchar8 result;

  result.s0 = EXTRACT_BITS(byte_triplets[0], 0, width_mask3bit);
  result.s1 = EXTRACT_BITS(byte_triplets[0], 3, width_mask3bit);
  result.s2 = EXTRACT_BITS(byte_triplets[0], 6, width_mask3bit);
  result.s2 = (result.s2 << 1) | (byte_triplets[1] & 1);

  result.s3 = EXTRACT_BITS(byte_triplets[1], 1, width_mask3bit);
  result.s4 = EXTRACT_BITS(byte_triplets[1], 4, width_mask3bit);
  result.s5 = byte_triplets[1] >> 7;
  result.s5 = (result.s5 << 2) | (byte_triplets[2] & 3);

  result.s6 = EXTRACT_BITS(byte_triplets[2], 2, width_mask3bit);
  result.s7 = EXTRACT_BITS(byte_triplets[2], 5, width_mask3bit);

  return result;
}

unsigned char octtree_node_id_get_subnode_id(octtree_node_id id, int level)
{




  return subkey
}

octtree_bucket_id octtree_hash_node_id(octtree_data_ctx* ctx, octtree_node_id id)
{
  int* intid = (int*)&id;

  // Todo: Improve locality by shifting key triplets such that the leaf triplet
  // is at the end (and not followed by zeroes)
  octtree_bucket_id combined_components = intid[0];
  combined_components ^= intid[1];
  combined_components ^= intid[2];
  combined_components ^= intid[3];

  return combined_components % ctx->num_buckets;
}

octtree_cell_data octtree_get_bucket(octtree_data_ctx* ctx, octtree_bucket_id bucket)
{
  octtree_cell_data result;
  result.data = ctx->data[bucket];
  result.key = ctx->keys[bucket];
  result.next = ctx->next[bucket];
}

octtree_cell_data octtree_retrieve_cell(octtree_data_ctx* ctx, octtree_node_id)
{

}

octtree_node_id octtree_get_next_node_on_line(octtree_data_ctx* ctx,
                                                  octtree_node_id current_id,
                                                  vector3 line_origin,
                                                  vector3 line_direction)
{

}


#endif
