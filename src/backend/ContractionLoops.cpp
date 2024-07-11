#include "ContractionLoops.h"

#include <algorithm>
//TODO remove
#include <iostream>

#ifdef _OPENMP
#include <omp.h>
#endif

void einsum_ir::backend::ContractionLoops::init( int64_t                              i_num_dims_c,
                                                 int64_t                              i_num_dims_m,
                                                 int64_t                              i_num_dims_n,
                                                 int64_t                              i_num_dims_k,
                                                 int64_t                      const * i_dim_ids_c,
                                                 int64_t                      const * i_dim_ids_m,
                                                 int64_t                      const * i_dim_ids_n,
                                                 int64_t                      const * i_dim_ids_k,
                                                 std::map< int64_t, int64_t > const * i_sizes,
                                                 std::map< int64_t, int64_t > const * i_strides_left,
                                                 std::map< int64_t, int64_t > const * i_strides_right,
                                                 std::map< int64_t, int64_t > const * i_strides_out_aux,
                                                 std::map< int64_t, int64_t > const * i_strides_out,
                                                 std::map< int64_t, dim_t >   const * i_dim_type,
                                                 int64_t                              i_num_bytes_scalar_left,
                                                 int64_t                              i_num_bytes_scalar_right,
                                                 int64_t                              i_num_bytes_scalar_out,
                                                 kernel_t                             i_ktype_first_touch,
                                                 kernel_t                             i_ktype_main,
                                                 kernel_t                             i_ktype_last_touch,
                                                 ContractionPackingTpp              * i_packing ) {
  m_num_dims_c = i_num_dims_c;
  m_num_dims_m = i_num_dims_m;
  m_num_dims_n = i_num_dims_n;
  m_num_dims_k = i_num_dims_k;

  m_dim_ids_c = i_dim_ids_c;
  m_dim_ids_m = i_dim_ids_m;
  m_dim_ids_n = i_dim_ids_n;
  m_dim_ids_k = i_dim_ids_k;

  m_sizes = i_sizes;

  m_strides_left = i_strides_left;
  m_strides_right = i_strides_right;
  m_strides_out_aux = i_strides_out_aux;
  m_strides_out = i_strides_out;

  m_num_bytes_scalar_left  = i_num_bytes_scalar_left;
  m_num_bytes_scalar_right = i_num_bytes_scalar_right;
  m_num_bytes_scalar_out   = i_num_bytes_scalar_out;

  m_ktype_first_touch = i_ktype_first_touch;
  m_ktype_main        = i_ktype_main;
  m_ktype_last_touch  = i_ktype_last_touch;

  m_num_tasks_targeted = 1;

  m_packing = i_packing;

  m_dim_type = i_dim_type;

  m_threading_first_last_touch = false;

  m_compiled = false;
}

einsum_ir::err_t einsum_ir::backend::ContractionLoops::compile() {
  //TODO i expect errors when combining packing and Complex numbers

  // determine if the outermost C dimension is complex
  m_cpx_outer_c = false;
  m_cpx_outer_c |= ce_cpx_op( m_ktype_first_touch );
  m_cpx_outer_c |= ce_cpx_op( m_ktype_main );
  m_cpx_outer_c |= ce_cpx_op( m_ktype_last_touch );

  // check if a complex C dimension is possible
  if( m_cpx_outer_c && m_num_dims_c == 0 ) {
    return err_t::INVALID_CPX_DIM;
  }
  if( m_cpx_outer_c &&  m_sizes->at( m_dim_ids_c[0] ) != 2 ) {
    return err_t::INVALID_CPX_DIM;
  }

  // derive complex strides
  if( m_cpx_outer_c ) {
    m_cpx_stride_in_left_bytes  = m_strides_left->at( m_dim_ids_c[0] )    * m_num_bytes_scalar_left;
    m_cpx_stride_in_right_bytes = m_strides_right->at( m_dim_ids_c[0] )   * m_num_bytes_scalar_right;
    m_cpx_stride_out_aux_bytes  = m_strides_out_aux->at( m_dim_ids_c[0] ) * m_num_bytes_scalar_out;
    m_cpx_stride_out_bytes      = m_strides_out->at( m_dim_ids_c[0] )     * m_num_bytes_scalar_out;
  }
  else {
    m_cpx_stride_in_left_bytes  = 0;
    m_cpx_stride_in_right_bytes = 0;
    m_cpx_stride_out_aux_bytes  = 0;
    m_cpx_stride_out_bytes      = 0;
  }

  // derive loop parameters for C dimension
  int64_t         l_num_dims_c = m_cpx_outer_c ? m_num_dims_c - 1 : m_num_dims_c;
  int64_t const * l_dim_ids_c  = m_cpx_outer_c ? m_dim_ids_c  + 1 : m_dim_ids_c;

  m_num_loops = l_num_dims_c + m_num_dims_m + m_num_dims_n + m_num_dims_k;

  //determine loop execution order
  std::vector< int64_t > l_loop_dims;
  std::vector< int64_t > l_packing_dims;
  l_loop_dims.reserve(m_num_loops);
  if( m_packing != nullptr ){
    l_packing_dims.reserve( m_packing->m_dim_ids_extra.size() );
    l_packing_dims.insert( l_packing_dims.end(), m_packing->m_dim_ids_extra.begin(), m_packing->m_dim_ids_extra.end() );
  }
  for( int64_t l_di = 0; l_di < l_num_dims_c; l_di++ ) {
    if( std::find( l_packing_dims.begin(), l_packing_dims.end(), l_dim_ids_c[l_di] ) == l_packing_dims.end() ) {
      l_loop_dims.push_back( l_dim_ids_c[l_di] );
    }
  }
  for( int64_t l_di = 0; l_di < m_num_dims_m; l_di++ ) {
    if( std::find( l_packing_dims.begin(), l_packing_dims.end(), m_dim_ids_m[l_di] ) == l_packing_dims.end() ) {
      l_loop_dims.push_back( m_dim_ids_m[l_di] );
    }
  }
  for( int64_t l_di = 0; l_di < m_num_dims_n; l_di++ ) {
    if( std::find( l_packing_dims.begin(), l_packing_dims.end(), m_dim_ids_n[l_di] ) == l_packing_dims.end() ) {
      l_loop_dims.push_back( m_dim_ids_n[l_di] );
    }
  }
  for( int64_t l_di = 0; l_di < m_num_dims_k; l_di++ ) {
    if( std::find( l_packing_dims.begin(), l_packing_dims.end(), m_dim_ids_k[l_di] ) == l_packing_dims.end() ) {
      l_loop_dims.push_back( m_dim_ids_k[l_di] );
    }
  }
  for( int64_t l_di = 0; l_di < l_packing_dims.size(); l_di++ ) {
    l_loop_dims.push_back( l_packing_dims[l_di] );
  }

  m_loop_first_last_touch.clear();
  m_loop_dim_type.clear();
  m_loop_sizes.clear();
  m_loop_strides_left.clear();
  m_loop_strides_right.clear();
  m_loop_strides_out_aux.clear();
  m_loop_strides_out.clear();

  m_loop_first_last_touch.reserve( m_num_loops );
  m_loop_dim_type.reserve(         m_num_loops );
  m_loop_sizes.reserve(            m_num_loops );
  m_loop_strides_left.reserve(     m_num_loops );
  m_loop_strides_right.reserve(    m_num_loops );
  m_loop_strides_out_aux.reserve(  m_num_loops );
  m_loop_strides_out.reserve(      m_num_loops );

  // add data of dimensions
  int64_t l_num_dim_cbm = l_num_dims_c + m_num_dims_m + m_num_dims_n;
  int64_t l_count_cbm = 0;
  int64_t l_count_k = 0;
  int64_t l_size_all_k = 0;
  bool l_found_first_last_touch_loop = false;
  for( int64_t l_di = 0; l_di < m_num_loops; l_di++ ) {
    int64_t l_dim_id = l_loop_dims[l_di];
    dim_t l_type = m_dim_type->at(l_dim_id);

    //add first_last_touch to correct loop
    if( l_type == einsum_ir::K ){
      l_count_k++;
      l_size_all_k += m_sizes->at(l_dim_id)-1;
      if( l_count_cbm == l_num_dim_cbm && !l_found_first_last_touch_loop ) {
        m_loop_first_last_touch.push_back( BEFORE_AFTER_ITER );
        l_found_first_last_touch_loop = true;
      }
      else {
        m_loop_first_last_touch.push_back( NONE );
      }
    }
    else{
      l_count_cbm++;
      if( l_count_cbm == l_num_dim_cbm && l_count_k == m_num_dims_k ){
        m_loop_first_last_touch.push_back( EVERY_ITER );
        l_found_first_last_touch_loop = true;
        m_loop_sum_k_sizes = l_size_all_k;
      }
      else {
        m_loop_first_last_touch.push_back( NONE );
      }
    }
    
    //setup loop structure
    m_loop_dim_type.push_back( l_type                );
    m_loop_sizes.push_back(    m_sizes->at(l_dim_id) );
    if(auto search = m_strides_left->find(l_dim_id); search != m_strides_left->end() ) {
      m_loop_strides_left.push_back(search->second);
    }
    else {
      m_loop_strides_left.push_back( 0 );
    }
        if(auto search = m_strides_right->find(l_dim_id); search != m_strides_right->end() ) {
      m_loop_strides_right.push_back(search->second);
    }
    else {
      m_loop_strides_right.push_back( 0 );
    }
        if(auto search = m_strides_out_aux->find(l_dim_id); search != m_strides_out_aux->end() ) {
      m_loop_strides_out_aux.push_back(search->second);
    }
    else {
      m_loop_strides_out_aux.push_back( 0 );
    }
        if(auto search = m_strides_out->find(l_dim_id); search != m_strides_out->end() ) {
      m_loop_strides_out.push_back(search->second);
    }
    else {
      m_loop_strides_out.push_back( 0 );
    }
  }

  // add dummy data for non-existing loops such that the inner kernel is executed
  if( m_num_loops == 0 ) {
    m_num_loops = 1;
    m_loop_dim_type.push_back(         dim_t::UNDEFINED_DIM );
    m_loop_first_last_touch.push_back( EVERY_ITER           );
    m_loop_sizes.push_back(            1                    );
    m_loop_strides_left.push_back(     0                    );
    m_loop_strides_right.push_back(    0                    );
    m_loop_strides_out_aux.push_back(  0                    );
    m_loop_strides_out.push_back(      0                    );
  }

  //setup extra packing loops
  m_id_packing_loop_left  = m_num_loops;
  m_id_packing_loop_right = m_num_loops;
  if( m_packing != nullptr ){
    m_id_packing_loop_left -= m_packing->m_packing_loop_offset_left;
    m_id_packing_loop_right -= m_packing->m_packing_loop_offset_right;
  }

  // scale with size of data types
  for( int64_t l_lo = 0; l_lo < m_num_loops; l_lo++ ) {
    m_loop_strides_left[l_lo]    *= m_num_bytes_scalar_left;
    m_loop_strides_right[l_lo]   *= m_num_bytes_scalar_right;
    m_loop_strides_out_aux[l_lo] *= m_num_bytes_scalar_out;
    m_loop_strides_out[l_lo]     *= m_num_bytes_scalar_out;
  }

  // compile iteration spaces
  err_t l_err = threading( m_num_tasks_targeted );
  if( l_err != err_t::SUCCESS ) {
    return err_t::COMPILATION_FAILED;
  }

  m_compiled = true;

  return err_t::SUCCESS;
}

einsum_ir::err_t einsum_ir::backend::ContractionLoops::threading( int64_t i_num_tasks ) {
  m_num_tasks = i_num_tasks;

  int64_t l_num_parallel = m_num_dims_c + m_num_dims_m + m_num_dims_n;
  l_num_parallel = m_cpx_outer_c ? l_num_parallel - 1 : l_num_parallel;

  m_iter_spaces.init( m_num_loops,
                      l_num_parallel,
                      nullptr,
                      m_loop_sizes.data(),
                      m_num_tasks );
  err_t l_err = m_iter_spaces.compile();
  if( l_err != err_t::SUCCESS ) {
    return err_t::COMPILATION_FAILED;
  }

  m_num_tasks = m_iter_spaces.num_tasks();

  return err_t::SUCCESS;
}

void einsum_ir::backend::ContractionLoops::contract_iter( int64_t         i_id_task,
                                                          int64_t         i_id_loop,
                                                          void    const * i_ptr_left,
                                                          void    const * i_ptr_right,
                                                          void    const * i_ptr_out_aux,
                                                          void          * i_ptr_out ) {
  // derive first element and number of iterations
  int64_t l_first = m_iter_spaces.firsts(i_id_task)[ i_id_loop ];
  int64_t l_size  = m_iter_spaces.sizes(i_id_task)[  i_id_loop ];

  // execute first touch kernel
  if( m_loop_first_last_touch[i_id_loop] == BEFORE_AFTER_ITER ) {
    kernel_first_touch( i_ptr_out_aux,
                        i_ptr_out );
  }

  // issue loop iterations
  for( int64_t l_it = l_first; l_it < l_first+l_size; l_it++ ) {
    char * l_ptr_left    = (char *) i_ptr_left;
    char * l_ptr_right   = (char *) i_ptr_right;
    char * l_ptr_out_aux = (char *) i_ptr_out_aux;
    char * l_ptr_out     = (char *) i_ptr_out;

    l_ptr_left    += l_it * m_loop_strides_left[    i_id_loop ];
    l_ptr_right   += l_it * m_loop_strides_right[   i_id_loop ];
    l_ptr_out_aux += l_it * m_loop_strides_out_aux[ i_id_loop ] * (l_ptr_out_aux != nullptr);
    l_ptr_out     += l_it * m_loop_strides_out[     i_id_loop ];

    if( m_loop_first_last_touch[i_id_loop] == EVERY_ITER ) {
      kernel_first_touch( l_ptr_out_aux,
                          l_ptr_out );
    }

    if( i_id_loop + 1 < m_num_loops ) {
      contract_iter( i_id_task,
                     i_id_loop+1,
                     l_ptr_left,
                     l_ptr_right,
                     l_ptr_out_aux,
                     l_ptr_out );
    }
    else {
      // execute main kernel
      kernel_main( l_ptr_left,
                   l_ptr_right,
                   l_ptr_out );
    }

    if( m_loop_first_last_touch[i_id_loop] == EVERY_ITER ) {
      kernel_last_touch( l_ptr_out_aux,
                         l_ptr_out );
    }
  }

  // execute last touch kernel
  if( m_loop_first_last_touch[i_id_loop] == BEFORE_AFTER_ITER ) {
    kernel_last_touch( i_ptr_out_aux,
                       i_ptr_out );
  }
}

void einsum_ir::backend::ContractionLoops::contract( void const * i_tensor_left,
                                                     void const * i_tensor_right,
                                                     void const * i_tensor_out_aux,
                                                     void       * io_tensor_out ) {

  if( m_packing != nullptr){
    m_packing->allocate_memory();
    
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for( int64_t l_ta = 0; l_ta < m_num_tasks; l_ta++ ) {
      if( m_id_packing_loop_left == -1 ){
        i_tensor_left = m_packing->kernel_pack_left( (char *) i_tensor_left );
      }
      if( m_id_packing_loop_right == -1 ){
        i_tensor_right = m_packing->kernel_pack_right( (char *) i_tensor_right );
      }
      contract_iter_packing( l_ta,
                             0,
                             0,
                             i_tensor_left,
                             i_tensor_right,
                             i_tensor_out_aux,
                             io_tensor_out );
    }
  }
else{
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for( int64_t l_ta = 0; l_ta < m_num_tasks; l_ta++ ) {
      contract_iter( l_ta,
                    0,
                    i_tensor_left,
                    i_tensor_right,
                    i_tensor_out_aux,
                    io_tensor_out );
    }
  }
}


void einsum_ir::backend::ContractionLoops::contract_iter_packing( int64_t         i_id_task,
                                                                  int64_t         i_id_loop,
                                                                  int64_t         i_id_k_loops,
                                                                  void    const * i_ptr_left,
                                                                  void    const * i_ptr_right,
                                                                  void    const * i_ptr_out_aux,
                                                                  void          * i_ptr_out ) {
  // derive first element and number of iterations
  int64_t l_first = m_iter_spaces.firsts(i_id_task)[ i_id_loop ];
  int64_t l_size  = m_iter_spaces.sizes(i_id_task)[  i_id_loop ];
  int64_t l_id_k_loops = i_id_k_loops;

  // execute first touch kernel
  if( m_loop_first_last_touch[i_id_loop] == BEFORE_AFTER_ITER ) {
    kernel_first_touch( i_ptr_out_aux,
                        i_ptr_out );
  }

  // issue loop iterations
  for( int64_t l_it = l_first; l_it < l_first+l_size; l_it++ ) {
    char * l_ptr_left    = (char *) i_ptr_left;
    char * l_ptr_right   = (char *) i_ptr_right;
    char * l_ptr_out_aux = (char *) i_ptr_out_aux;
    char * l_ptr_out     = (char *) i_ptr_out;

    l_ptr_left    += l_it * m_loop_strides_left[    i_id_loop ];
    l_ptr_right   += l_it * m_loop_strides_right[   i_id_loop ];
    l_ptr_out_aux += l_it * m_loop_strides_out_aux[ i_id_loop ] * (l_ptr_out_aux != nullptr);
    l_ptr_out     += l_it * m_loop_strides_out[     i_id_loop ];

    if( m_loop_first_last_touch[i_id_loop] == EVERY_ITER && i_id_k_loops == 0 ) {
      kernel_first_touch( l_ptr_out_aux,
                          l_ptr_out );
    }

    if( m_id_packing_loop_left == i_id_loop ){
      l_ptr_left = m_packing->kernel_pack_left( l_ptr_left );
    }
    if( m_id_packing_loop_right == i_id_loop ){
      l_ptr_right = m_packing->kernel_pack_right( l_ptr_right );
    }

    if( i_id_loop + 1 < m_num_loops ) {
      contract_iter_packing( i_id_task,
                             i_id_loop+1,
                             l_id_k_loops,
                             l_ptr_left,
                             l_ptr_right,
                             l_ptr_out_aux,
                             l_ptr_out );
    }
    else {
      // execute main kernel
      kernel_main( l_ptr_left,
                   l_ptr_right,
                   l_ptr_out );
    }
    if( m_loop_first_last_touch[i_id_loop] == EVERY_ITER && m_loop_sum_k_sizes == i_id_k_loops ) {
      kernel_last_touch( l_ptr_out_aux,
                         l_ptr_out );
    }
    l_id_k_loops += m_loop_dim_type[i_id_loop] == einsum_ir::K;
  }

  // execute last touch kernel
  if( m_loop_first_last_touch[i_id_loop] == BEFORE_AFTER_ITER ) {
    kernel_last_touch( i_ptr_out_aux,
                       i_ptr_out );
  }
}