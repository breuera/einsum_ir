#ifndef EINSUM_IR_BACKEND_CONTRACTION_LOOPS
#define EINSUM_IR_BACKEND_CONTRACTION_LOOPS

#include <cstdint>
#include <vector>
#include "../constants.h"

namespace einsum_ir {
  namespace backend {
    class ContractionLoops;
  }
}

class einsum_ir::backend::ContractionLoops {
  private:
    //! number of C dimensions
    int64_t m_num_dims_c = 0;
    //! number of M dimensions
    int64_t m_num_dims_m = 0;
    //! number of N dimensions
    int64_t m_num_dims_n = 0;
    //! number of K dimensions
    int64_t m_num_dims_k = 0;

    //! sizes of the C dimensions
    int64_t const * m_sizes_c = nullptr;
    //! sizes of the M dimensions
    int64_t const * m_sizes_m = nullptr;
    //! sizes of the N dimensions
    int64_t const * m_sizes_n = nullptr;
    //! sizes of the K dimensions
    int64_t const * m_sizes_k = nullptr;

    //! C strides of the left input tensor
    int64_t const * m_strides_in_left_c = nullptr;
    //! M strides of the left input tensor
    int64_t const * m_strides_in_left_m = nullptr;
    //! K strides of the left input tensor
    int64_t const * m_strides_in_left_k = nullptr;

    //! C strides of the right input tensor
    int64_t const * m_strides_in_right_c = nullptr;
    //! N strides of the right input tensor
    int64_t const * m_strides_in_right_n = nullptr;
    //! K strides of the right input tensor
    int64_t const * m_strides_in_right_k = nullptr;

    //! C strides of the output tensor
    int64_t const * m_strides_out_c = nullptr;
    //! M strides of the output tensor
    int64_t const * m_strides_out_m = nullptr;
    //! N strides of the output tensor
    int64_t const * m_strides_out_n = nullptr;

    //! number of bytes for a scalar of the left input tensor
    int64_t m_num_bytes_scalar_left = 0;
    //! number of bytes for a scalar of the right input tensor
    int64_t m_num_bytes_scalar_right = 0;
    //! number of bytes for a scalar of the output tensor
    int64_t m_num_bytes_scalar_out = 0;

    //! number of loops used for threading
    int64_t m_threading_num_loops = -1;
    //! true if the threading loops have to take care of fist/last touch ops
    bool m_threading_first_last_touch = false;

    //! first/last touch type of a loop
    typedef enum {
      // no first/last touch
      NONE = 0,
      // first touch before and last touch after the loop
      BEFORE_AFTER_ITER = 1,
      // first touch before the main kernel in every iteration
      // last touch after the main kernel in very iteration
      EVERY_ITER = 2
    } touch_t;

    //! number of loos
    int64_t m_num_loops = -1;
    //! first/last touch type of the loops
    std::vector< touch_t > m_loop_first_last_touch;
    //! dimension types of the loops (C, M, N or K)
    std::vector< dim_t >   m_loop_dim_type;
    //! sizes of the loops / number of iterations
    std::vector< int64_t > m_loop_sizes;
    //! per-loop-iteration stride in byte w.r.t. the left tensor
    std::vector< int64_t > m_loop_strides_left;
    //! per-loop-iteration stride in byte w.r.t. the right tensor
    std::vector< int64_t > m_loop_strides_right;
    //! per-loop-iteration stride in byte w.r.t. the output tensor
    std::vector< int64_t > m_loop_strides_out;

    //! true if the contraction loop interface was compiled
    bool m_compiled = false;

  public:
    /**
     * Kernel applied to the output tensor before the contraction.
     *
     * @param io_out pointer to a data section of the output tensor.
     **/
    virtual void kernel_first_touch( void * io_out ) = 0;

    /**
     * Kernel applied to the output tensor after the contraction.
     *
     * @param io_out pointer to a data section of the output tensor.
     **/
    virtual void kernel_last_touch( void * io_out ) = 0;

    /**
     * Kernel called in the innermost loop.
     *
     * @param i_left pointer to a data section of the left tensor.
     * @param i_right pointer to a data section of the right tensor.
     * @param io_out pointer to a data section of the output tensor.
     **/
    virtual void kernel_inner( void const * i_left,
                               void const * i_right,
                               void       * io_out ) = 0;

    /**
     * Initializes the the class.
     *
     * Shortcuts:
     *   C: batch dimensions which appears in all tensors.
     *   M: dimensions appear in left input and output.
     *   N: dimensions appear in right input and output.
     *   K: reduction dimensions which appear in both inputs,
     *
     * @param i_num_dims_c number of C dimensions.
     * @param i_num_dims_m number of M dimensions.
     * @param i_num_dims_n number of N dimensions.
     * @param i_num_dims_k number of K dimensions.
     * @param i_sizes_c sizes of the C dimensions.
     * @param i_sizes_m sizes of the M dimensions.
     * @param i_sizes_n sizes of the N dimensions.
     * @param i_sizes_k sizes of the K dimensions.
     * @param i_strides_in_left_c C strides of the left input tensor.
     * @param i_strides_in_left_m M strides of the left input tensor.
     * @param i_strides_in_left_k K strides of the left input tensor.
     * @param i_strides_in_right_c C strides of the right input tensor.
     * @param i_strides_in_right_n N strides of the right input tensor.
     * @param i_strides_in_right_k K strides of the right input tensor.
     * @param i_strides_out_c C strides of the output tensor.
     * @param i_strides_out_m M strides of the output tensor.
     * @param i_strides_out_n N strides of the output tensor.
     * @param i_num_bytes_scalar_left number of bytes per scalar in the left tensor.
     * @param i_num_bytes_scalar_right number of bytes per scalar in the right tensor.
     * @param i_num_bytes_scalar_out number of bytes per scalar in the output tensor.
     **/
    void init( int64_t         i_num_dims_c,
               int64_t         i_num_dims_m,
               int64_t         i_num_dims_n,
               int64_t         i_num_dims_k,
               int64_t const * i_sizes_c,
               int64_t const * i_sizes_m,
               int64_t const * i_sizes_n,
               int64_t const * i_sizes_k,
               int64_t const * i_strides_in_left_c,
               int64_t const * i_strides_in_left_m,
               int64_t const * i_strides_in_left_k,
               int64_t const * i_strides_in_right_c,
               int64_t const * i_strides_in_right_n,
               int64_t const * i_strides_in_right_k,
               int64_t const * i_strides_out_c,
               int64_t const * i_strides_out_m,
               int64_t const * i_strides_out_n,
               int64_t         i_num_bytes_scalar_left,
               int64_t         i_num_bytes_scalar_right,
               int64_t         i_num_bytes_scalar_out );

    /**
     * Compiles the contraction loop interface.
     *
     * @return SUCCESS if the compilation was successful, otherwise an appropiate error code.
     **/
    err_t compile();

    /**
     * Derives the threading data for the contraction loops.
     * Parallelizes all loops such that the targeted number of tasks is reached or
     * all parallelizable loop dimensions have been exhausted.
     *
     * @param i_num_tasks_target number of targeted tasks. 
     **/
    void threading( int64_t i_num_tasks_target );

    /**
     * General purpose loop implementation featuring first and last touch operations.
     * No threading is applied.
     *
     * @param i_id_loop dimension id of the loop which is executed.
     * @param i_ptr_left pointer to the left tensor's data.
     * @param i_ptr_right pointer to the right tensor's data.
     * @param i_ptr_out pointer to the output tensor's data.
     **/
    void contract_iter( int64_t         i_id_loop,
                        void    const * i_ptr_left,
                        void    const * i_ptr_right,
                        void          * i_ptr_out );

    /**
     * Threaded loop implementation featuring first and last touch operation.
     * The outermost loop is parallelized, then contact_iter is called.
     *
     * @param i_id_loop dimension id of the loop which is executed.
     * @param i_ptr_left pointer to the left tensor's data.
     * @param i_ptr_right pointer to the right tensor's data.
     * @param i_ptr_out pointer to the output tensor's data.
     **/
    void contract_iter_parallel_touch_1( int64_t         i_id_loop,
                                         void    const * i_ptr_left,
                                         void    const * i_ptr_right,
                                         void          * i_ptr_out );

    /**
     * Threaded loop implementation featuring first and last touch operation.
     * The 2x outermost loops are parallelized (collapsed), then contact_iter is called.
     * BEFORE_AFTER_ITER fist/last touch is only support w.r.t. the outermost loop.
     *
     * @param i_id_loop dimension id of the loop which is executed.
     * @param i_ptr_left pointer to the left tensor's data.
     * @param i_ptr_right pointer to the right tensor's data.
     * @param i_ptr_out pointer to the output tensor's data.
     **/
    void contract_iter_parallel_touch_2( int64_t         i_id_loop,
                                         void    const * i_ptr_left,
                                         void    const * i_ptr_right,
                                         void          * i_ptr_out );

    /**
     * Threaded loop implementation without first/last touch support.
     * The outermost loop is parallelized, then contact_iter is called.
     *
     * @param i_id_loop dimension id of the loop which is executed.
     * @param i_ptr_left pointer to the left tensor's data.
     * @param i_ptr_right pointer to the right tensor's data.
     * @param i_ptr_out pointer to the output tensor's data.
     **/
    void contract_iter_parallel_1( int64_t         i_id_loop,
                                   void    const * i_ptr_left,
                                   void    const * i_ptr_right,
                                   void          * i_ptr_out );

    /**
     * Threaded loop implementation without first/last touch support.
     * The 2x outermost loops are parallelized, then contact_iter is called.
     *
     * @param i_id_loop dimension id of the loop which is executed.
     * @param i_ptr_left pointer to the left tensor's data.
     * @param i_ptr_right pointer to the right tensor's data.
     * @param i_ptr_out pointer to the output tensor's data.
     **/
    void contract_iter_parallel_2( int64_t         i_id_loop,
                                   void    const * i_ptr_left,
                                   void    const * i_ptr_right,
                                   void          * i_ptr_out );

    /**
     * Threaded loop implementation without first/last touch support.
     * The 3x outermost loops are parallelized, then contact_iter is called.
     *
     * @param i_id_loop dimension id of the loop which is executed.
     * @param i_ptr_left pointer to the left tensor's data.
     * @param i_ptr_right pointer to the right tensor's data.
     * @param i_ptr_out pointer to the output tensor's data.
     **/
    void contract_iter_parallel_3( int64_t         i_id_loop,
                                   void    const * i_ptr_left,
                                   void    const * i_ptr_right,
                                   void          * i_ptr_out );

    /**
     * Threaded loop implementation without first/last touch support.
     * The 4x outermost loops are parallelized, then contact_iter is called.
     *
     * @param i_id_loop dimension id of the loop which is executed.
     * @param i_ptr_left pointer to the left tensor's data.
     * @param i_ptr_right pointer to the right tensor's data.
     * @param i_ptr_out pointer to the output tensor's data.
     **/
    void contract_iter_parallel_4( int64_t         i_id_loop,
                                   void    const * i_ptr_left,
                                   void    const * i_ptr_right,
                                   void          * i_ptr_out );

    /**
     * Contracts the two input tensors.
     * Uses C-M-N-K (outer-to-inner dimensions) for the ordering.
     *
     * @param i_dim_type dimension type of the current recursion level: C (0), M (1), N (2) or K (3).
     * @param i_dim_count counter for the current dimension type.
     * @param i_ptr_in_left pointer to the left tensor.
     * @param i_ptr_in_right pointer to the right tensor.
     * @param i_ptr_out pointer to the output tensor.
     **/
    void contract_cnmk( char            i_dim_type,
                        int64_t         i_dim_count,
                        void    const * i_ptr_in_left,
                        void    const * i_ptr_in_right,
                        void          * i_ptr_out );

    /**
     * Contracts the two tensors.
     *
     * @param i_tensor_in_left left tensor.
     * @param i_tensor_in_right right tensor.
     * @param io_tensor_out output tensor.
     **/
    void contract( void const * i_tensor_in_left,
                   void const * i_tensor_in_right,
                   void       * io_tensor_out );
};

#endif