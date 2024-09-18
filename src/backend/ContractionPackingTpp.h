#ifndef EINSUM_IR_BACKEND_CONTRACTION_PACKING_TPP
#define EINSUM_IR_BACKEND_CONTRACTION_PACKING_TPP

#include <vector>
#include <map>
#include "UnaryTpp.h"
#include "MemoryManager.h"
#include "../constants.h"

namespace einsum_ir {
  namespace backend {
    class ContractionPackingTpp;
  }
}

class einsum_ir::backend::ContractionPackingTpp {
  public:
    //! number of dimensions of the left tensor
    int64_t m_num_dims_left = 0;
    //! number of dimensions of the right tensor
    int64_t m_num_dims_right = 0;

    //! loop offset from last loop for accurate packing call
    int64_t m_packing_loop_offset_left = 0;
    //! number of non kernel dimensions required for packing of right tensor
    int64_t m_packing_loop_offset_right = 0;

    //! required memory in bytes for packing of left tensor
    int64_t m_size_packing_left = 0;
    //! required memory in bytes for packing of right tensor
    int64_t m_size_packing_right = 0;
    

    //! mapping from the dimension ids to the inner dimension sizes
    std::map< int64_t, int64_t > const * m_dim_sizes = nullptr;

    //! mapping from the dimension ids to strides of left tensor
    std::map< int64_t, int64_t > const * m_strides_left = nullptr;

    //! mapping from the dimension ids to strides of right tensor
    std::map< int64_t, int64_t > const * m_strides_right = nullptr;

    //! mapping from the dimension ids to dimension type of contactoin
    std::map< int64_t, dim_t > const * m_dim_type = nullptr;

    //! mapping from the dimension ids to strides of left tensor
    std::map< int64_t, int64_t > m_strides_packed_left;

    //! mapping from the dimension ids to strides of right tensor
    std::map< int64_t, int64_t > m_strides_packed_right;
 

    //! left tensor's dimension ids
    int64_t const * m_dim_ids_left = nullptr;
    //! right tensor's dimension ids
    int64_t const * m_dim_ids_right = nullptr;

    //! loop execution order
    std::vector< int64_t > const *  m_loop_dims;

    //! permutation of dimension for the left tensor
    std::vector< int64_t > const * m_dim_ids_kernel_left;
    //! permutation of dimension for the right tensor
    std::vector< int64_t > const * m_dim_ids_kernel_right;

    //! datatype of the left input
    data_t m_dtype_left = UNDEFINED_DTYPE;

    //! datatype of the right input
    data_t m_dtype_right = UNDEFINED_DTYPE;

    //! unary packing kernel for left tensor
    UnaryTpp * m_unary_left = nullptr;

    //! unary packing kernel for right tensor
    UnaryTpp * m_unary_right = nullptr;

    //! Memory manager for allocation of memory
    MemoryManager * m_memory;

    //! array of memory pointers for thread specific packing memory
    char ** m_memory_packing;

    //TODO add description
    void init( int64_t                              i_num_dims_left,
               int64_t                              i_num_dims_right,
               std::map< int64_t, int64_t > const * i_dim_sizes,
               std::map< int64_t, int64_t > const * i_strides_left,
               std::map< int64_t, int64_t > const * i_strides_right,
               std::map< int64_t, dim_t >   const * i_dim_type,
               int64_t                      const * i_dim_ids_left,
               int64_t                      const * i_dim_ids_right,
               std::vector< int64_t >       const * i_dim_ids_kernel_left,
               std::vector< int64_t >       const * i_dim_ids_kernel_right,
               std::vector< int64_t >       const * i_loop_dims,
               data_t                               i_dtype_left,
               data_t                               i_dtype_right,
               MemoryManager                      * i_memory );

    err_t compile();
    
    //TODO add description
    err_t create_kernel( int64_t                              i_num_dims,
                         int64_t                      const * i_dim_ids_original,
                         std::vector< int64_t >       const * i_dim_ids_packed,
                         std::map< int64_t, int64_t > const * i_strides_original,
                         std::map< int64_t, int64_t >       * o_strides_packed,
                         std::map< int64_t, int64_t > const * i_dim_sizes,
                         data_t                               i_dtype,
                         int64_t                            * o_size_packing,
                         UnaryTpp                           * o_unary );

    /**
     * Kernel to pack the left input tensor of the main kernel.
     *
     * @param i_in  pointer to a data section of the input tensor.
     * 
     * @return  pointer to packed data
     **/
    char * kernel_pack_left( char * i_in );
    
    /**
     * Kernel to pack the right input tensor of the main kernel.
     *
     * @param i_in  pointer to a data section of the input tensor.
     * 
     * @return   pointer to packed data
     **/
    char * kernel_pack_right( char * i_in );

    /**
     * called during evaluation of contraction to get memory from memory manager
    **/
    void allocate_memory();


};

#endif


