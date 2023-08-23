#ifndef EINSUM_IR_BACKEND_BINARY_CONTRACTION_SCALAR
#define EINSUM_IR_BACKEND_BINARY_CONTRACTION_SCALAR

#include "BinaryContraction.h"

namespace einsum_ir {
  namespace backend {
    class BinaryContractionScalar;
  }
}

class einsum_ir::backend::BinaryContractionScalar: public BinaryContraction {
  private:
    /**
     * Compiler-based zero kernel.
     *
     * @param_t datatype.
     * @param o_data data which is zeroed.
     **/
    template < typename T >
    static void kernel_zero( void * o_data );

    /**
     * Compiler-based ReLU kernel.
     *
     * @param_t datatype.
     * @param io_data data to which the ReLU is applied.
     **/
    template < typename T >
    static void kernel_relu( void * io_data );

    /**
     * Compiler-based multiply add kernel.
     *
     * @param_t T_LEFT data type of the left input.
     * @param_t T_RIGHT data type of the right input.
     * @param_t T_OUT data type of the output.
     **/
    template < typename T_LEFT,
               typename T_RIGHT,
               typename T_OUT >
    static void kernel_madd( void const * i_in_left,
                             void const * i_in_right,
                             void       * io_out );

    //! first-touch kernel
    void (* m_kernel_first_touch)( void * ) = nullptr;

    //! inner kernel
    void (* m_kernel_inner)( void const *,
                             void const *,
                             void       * ) = nullptr;

    //! last-touch kernel
    void (* m_kernel_last_touch)( void * ) = nullptr;

  public:
    /**
     * Compiles the binary contraction.
     *
     * @param i_tensor_ordering used ordering of the input tensors.
     * @return SUCCESS if successful, error code otherwise.
     **/
    err_t compile( tenord_t i_tensor_ordering );

    /**
     * Compiles the binary contraction.
     * @return SUCCESS if successful, error code otherwise.
     **/
    err_t compile();

    /**
     * Performs a contraction on the given input data.
     *
     * @param i_tensor_in_left left input tensor.
     * @param i_tensor_in_right right input tensor.
     * @param io_tensor_out output tensor.
     **/
    void contract( void const * i_tensor_in_left,
                   void const * i_tensor_in_right,
                   void       * io_tensor_out );
};

#endif
