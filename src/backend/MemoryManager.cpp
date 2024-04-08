#include "MemoryManager.h"
#include <memory>

einsum_ir::backend::MemoryManager::~MemoryManager() {
  if(  m_memory_ptr != nullptr ) {
    delete [] (char *)  m_memory_ptr;
  }
}

int64_t einsum_ir::backend::MemoryManager::reserve_memory(int64_t i_size){
  m_last_id++;

  //actual allocated size must be a multiple of alignement
  if( i_size % m_alignement_line != 0 ){
      i_size += m_alignement_line - ( i_size % m_alignement_line );
    }
  
  //caclculate new memory offset and append to allocation
  int64_t l_mem_id;
  int64_t l_offset = 0;
  if( m_layer_id % 2 == 0 ){
    if( !m_allocated_id_left.empty() ){
      l_offset = m_allocated_offset_left.front();
    }
    m_tensor_offset.push_back(l_offset);
    l_offset += i_size;
    l_mem_id = m_last_id;
    m_allocated_id_left.push_front(l_mem_id);
    m_allocated_offset_left.push_front(l_offset);
  }
  else{
    if( !m_allocated_id_right.empty() ){
      l_offset = m_allocated_offset_right.front();
    }
    l_offset -= i_size;
    l_mem_id = -m_last_id;
    m_tensor_offset.push_back(l_offset);
    m_allocated_id_right.push_front(l_mem_id);
    m_allocated_offset_right.push_front(l_offset);
  }

  //check if more memory required
  int64_t l_offset_left  = (m_allocated_offset_left.empty())  ? 0 : m_allocated_offset_left.front();
  int64_t l_offset_right = (m_allocated_offset_right.empty()) ? 0 : m_allocated_offset_right.front();
  int64_t l_current_mem = l_offset_left - l_offset_right;
  if(l_current_mem > m_req_mem){
    m_req_mem = l_current_mem;
  }

  return l_mem_id;
}

void einsum_ir::backend::MemoryManager::remove_reservation(int64_t i_id){

  //find offset and id in list of allocated and delete them
  std::list<int64_t>::iterator l_alloc_id_it;
  std::list<int64_t>::iterator l_alloc_offset_it;
  if( i_id >= 0 ){
    l_alloc_offset_it = m_allocated_offset_left.begin();
    for(l_alloc_id_it = m_allocated_id_left.begin(); l_alloc_id_it != m_allocated_id_left.end(); ++l_alloc_id_it ) {
      if( i_id == *l_alloc_id_it ) {
        break;
      }
      l_alloc_offset_it++;
    }
    m_allocated_id_left.erase(l_alloc_id_it);
    m_allocated_offset_left.erase(l_alloc_offset_it);
  }
  else {
    l_alloc_offset_it = m_allocated_offset_right.begin();
    for(l_alloc_id_it = m_allocated_id_right.begin(); l_alloc_id_it != m_allocated_id_right.end(); ++l_alloc_id_it ) {
      if( i_id == *l_alloc_id_it ) {
        break;
      }
      l_alloc_offset_it++;
    }
    m_allocated_id_right.erase(l_alloc_id_it);
    m_allocated_offset_right.erase(l_alloc_offset_it);
  }
}

void einsum_ir::backend::MemoryManager::alloc_all_memory(){
  if( m_req_mem > 0 ){
    //allocate memory 
    char * l_data = new char[m_req_mem + m_alignement_page];
    m_memory_ptr = l_data;

    //allign data in memory 
    m_aligned_memory_ptr = m_memory_ptr;
    int64_t l_align_offset = (unsigned long)l_data % m_alignement_page;
    if( l_align_offset ){
      m_aligned_memory_ptr += m_alignement_page - l_align_offset;
    }
  }
}

void * einsum_ir::backend::MemoryManager::get_mem_ptr(int64_t i_id){

  void * l_return_ptr;
  if(i_id >= 0){
    l_return_ptr = (void *) (m_aligned_memory_ptr +  m_tensor_offset[i_id - 1]);
  }
  else{
    l_return_ptr = (void *) (m_aligned_memory_ptr + m_req_mem + m_tensor_offset[-i_id - 1]);
  }
  return l_return_ptr;
}

void einsum_ir::backend::MemoryManager::increase_layer(){
  m_layer_id++;
}

void einsum_ir::backend::MemoryManager::decrease_layer(){
  m_layer_id--;
}
