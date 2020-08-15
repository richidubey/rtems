/**
 * @file
 *
 * @ingroup RTEMSScoreSchedulerStrongAPA
 *
 * @brief Strong APA Scheduler Implementation
 */

/*  
 * Copyright (c) 2020 Richi Dubey
 *
 * <richidubey@gmail.com>
 *
 * Copyright (c) 2013, 2018 embedded brains GmbH. All rights reserved.
 *
 *  embedded brains GmbH	
 *  Dornierstr. 4	
 *  82178 Puchheim	
 *  Germany	
 *  <rtems@embedded-brains.de>	
 *	
 * The license and distribution terms for this file may be	
 * found in the file LICENSE in this distribution or at	
 * http://www.rtems.org/license/LICENSE.
 */
 
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/score/schedulerstrongapa.h>
#include <rtems/score/schedulersmpimpl.h>
#include <rtems/score/assert.h>
#include <rtems/malloc.h>

static inline Scheduler_strong_APA_Context *
_Scheduler_strong_APA_Get_context( const Scheduler_Control *scheduler )
{
  return (Scheduler_strong_APA_Context *) _Scheduler_Get_context( scheduler );
}

static inline Scheduler_strong_APA_Context *
_Scheduler_strong_APA_Get_self( Scheduler_Context *context )
{
  return (Scheduler_strong_APA_Context *) context;
}

static inline Scheduler_strong_APA_Node *
_Scheduler_strong_APA_Node_downcast( Scheduler_Node *node )
{
  return (Scheduler_strong_APA_Node *) node;
}

static inline void _Scheduler_strong_APA_Do_update(
  Scheduler_Context *context,
  Scheduler_Node    *node,
  Priority_Control   new_priority
)
{
  Scheduler_SMP_Node *smp_node;
  (void) context;

  smp_node = _Scheduler_SMP_Node_downcast( node );
  _Scheduler_SMP_Node_update_priority( smp_node, new_priority );
}

static inline bool _Scheduler_strong_APA_Has_ready( Scheduler_Context *context )
{
  Scheduler_strong_APA_Context *self = _Scheduler_strong_APA_Get_self( context );
	
  bool               ret;
  const Chain_Node  *tail;
  Chain_Node        *next;
  Scheduler_strong_APA_Node *node;
  
  tail = _Chain_Immutable_tail( &self->All_nodes );
  next = _Chain_First( &self->All_nodes );
  
  ret = false;
  
  while ( next != tail ) {
    node = (Scheduler_strong_APA_Node *) next;
     
    if ( 
    _Scheduler_SMP_Node_state( &node->Base.Base ) == SCHEDULER_SMP_NODE_READY 
    ) {
      ret = true;
      break;
    }
    
    next = _Chain_Next( next );
  }
  
  return ret;
}

static inline void _Scheduler_strong_APA_Allocate_processor(
  Scheduler_Context *context,
  Scheduler_Node    *scheduled_base,
  Scheduler_Node    *victim_base,
  Per_CPU_Control   *victim_cpu
)
{
  Scheduler_strong_APA_Node *scheduled;
 
  (void) victim_base;
  scheduled = _Scheduler_strong_APA_Node_downcast( scheduled_base );

  _Scheduler_SMP_Allocate_processor_exact(
    context,
    &(scheduled->Base.Base),
    NULL,
    victim_cpu
  );
}

static inline Scheduler_Node *_Scheduler_strong_APA_Get_highest_ready(
  Scheduler_Context *context,
  Scheduler_Node    *filter
) //TODO
{
  //return the highest ready Scheduler_Node and Scheduler_Node filter here points
  // to the victim node that is blocked resulting which this function is called.
  Scheduler_strong_APA_Context *self=_Scheduler_strong_APA_Get_self( context );
	
  Thread_Control   *thread;
  Per_CPU_Control  *thread_cpu;
  Per_CPU_Control  *curr_CPU;
  Scheduler_strong_APA_Node *node;
  Per_CPU_Control  *assigned_cpu;
  uint32_t          index_assigned_cpu;
  Scheduler_Node    *highest_ready;
  Priority_Control  min_priority_num;
  Priority_Control  curr_priority;
  Scheduler_SMP_Node_state curr_state;
  Scheduler_Node *curr_node;
  Scheduler_Node *next_node;
  const Chain_Node *tail;
  Chain_Node       *next;
  Scheduler_strong_APA_Struct *Struct;
  
  //Denotes front and rear of the queue
  uint32_t	front;
  uint32_t	rear;
  
  uint32_t	cpu_max;
  uint32_t	cpu_index;
  
  front = 0;
  rear = -1;  

  thread = filter->user;	
  thread_cpu = _Thread_Get_CPU( thread );
	
  //Implement the BFS Algorithm for task departure
  //to get the highest ready task for a particular CPU
  
  //Initialize the min_priority_num variable
  min_priority_num = _Scheduler_Node_get_priority( filter );
  min_priority_num = SCHEDULER_PRIORITY_PURIFY( min_priority_num );

  highest_ready = filter;
  Struct = self->Struct;
  cpu_max = _SMP_Get_processor_maximum();
  
  for ( cpu_index = 0 ; cpu_index < cpu_max ; ++cpu_index ) { 
   Struct[ cpu_index ].visited = false;
  }
  
  rear = rear + 1;
  Struct[ rear ].Cpu = thread_cpu;
  Struct[ _Per_CPU_Get_index( thread_cpu ) ].visited = true;	
  
   while( front <= rear ) {
     curr_CPU = Struct[ front ].Cpu; 
     front = front + 1;

     tail = _Chain_Immutable_tail( &self->All_nodes );
     next = _Chain_First( &self->All_nodes );
  
     while ( next != tail ) {
       node = (Scheduler_strong_APA_Node *) next;
       curr_node = (Scheduler_Node *) next;
       curr_state = _Scheduler_SMP_Node_state( &node->Base.Base );
    
       if (
         _Processor_mask_Is_set(&node->Affinity, _Per_CPU_Get_index(curr_CPU) ||
        //2nd condition is a hack for now. Since if it has no affinity (false case
        // actually means has all affinity), then it has affinty for this cpu as well. Yay 
         _Processor_mask_Is_zero( &node->Affinity ) )
       ) {
       
         //Checks if the curr_CPU is in the affinity set of the node
          
         if ( curr_state == SCHEDULER_SMP_NODE_SCHEDULED ) {
           assigned_cpu = _Thread_Get_CPU( node->Base.Base.user );
           index_assigned_cpu =  _Per_CPU_Get_index( assigned_cpu );
         
           if ( Struct[ index_assigned_cpu ].visited == false ) {
            rear = rear + 1;
            Struct[ rear ].Cpu = assigned_cpu;
            Struct[ index_assigned_cpu ].visited = true;
            // The curr CPU of the queue invoked this node to add its CPU 
            // that it is executing on to the queue. So this node might get
            // preempted because of the invoker curr_CPU and this curr_CPU
            // is the CPU that node should preempt in case this node 
            // gets preempted.
            node->invoker = curr_CPU; 
           } 
         } 
         else if ( curr_state == SCHEDULER_SMP_NODE_READY ) {
           curr_priority = _Scheduler_Node_get_priority( (Scheduler_Node *) next );
  	   curr_priority = SCHEDULER_PRIORITY_PURIFY( curr_priority );
  	
           if ( curr_priority < min_priority_num ) {
             min_priority_num = curr_priority;
  	     highest_ready = &node->Base.Base;
  	   }
         }
       }
       next = _Chain_Next( next );
     }
   }
  
  if ( highest_ready != filter ) { //Should always be true since highest ready 
  // node corresponding to an empty cpu must exist: TODO: See how.
  
    //Backtrack on the path from
    //thread_cpu to highest_ready, shifting along every task.
    
    node = _Scheduler_strong_APA_Node_downcast( highest_ready );
    
    if( node->invoker != thread_cpu ) {
      // Highest ready is not just directly reachable from the victim cpu
      // So there is need of task shifting 
    
      do {
        curr_node = &node->Base.Base;
                //TODO: Put this in a module since it breaks line length
        next_node = _Thread_Scheduler_get_home_node( node->invoker->executing );
    
        _Scheduler_SMP_Preempt(
          context,
          curr_node,
          _Thread_Scheduler_get_home_node( node->invoker->executing ),
          _Scheduler_strong_APA_Allocate_processor
        );
     
       node = _Scheduler_strong_APA_Node_downcast( next_node );      
     }while( node->invoker !=  thread_cpu );
      //To save the last node so that the caller SMP_* function 
      //can do the allocation
    
      curr_node = &node->Base.Base;
      highest_ready = curr_node;  
    }
  }
  
  return highest_ready; 
}

static inline Scheduler_Node *_Scheduler_strong_APA_Get_lowest_scheduled(
  Scheduler_Context *context,
  Scheduler_Node    *filter_base
)
{	
  //Idea: BFS Algorithm for task arrival
	
  uint32_t	cpu_max;
  uint32_t	cpu_index;
  Priority_Control  filter_priority;
  
  Per_CPU_Control *curr_CPU;
  Per_CPU_Control *next_CPU;
  Per_CPU_Control *cpu_to_preempt;
  Thread_Control  *curr_thread;
  Scheduler_Node  *curr_node;
  Scheduler_Node  *ret;
  Priority_Control max_priority_num;
  Priority_Control curr_priority;

  Scheduler_strong_APA_Node *curr_strong_node; //Current Strong_APA_Node
  Scheduler_strong_APA_Node *filter_node;  
  Scheduler_strong_APA_Context *self;
  Scheduler_strong_APA_Struct *Struct; 
  
  //Denotes front and rear of the queue
  uint32_t	front;
  uint32_t	rear;
  
  front = 0;
  
  rear = -1;  

  self = _Scheduler_strong_APA_Get_self( context );
  Struct = self->Struct;
  
  filter_priority = _Scheduler_Node_get_priority( filter_base );
  filter_priority = SCHEDULER_PRIORITY_PURIFY( filter_priority );   
    
  ret = NULL; //To remove compiler warning. 
  //ret would always point to the node with the lowest priority
  //node unless the affinity of filter_base is NULL.
       
  filter_node = _Scheduler_strong_APA_Node_downcast( filter_base );
  
  max_priority_num = 0;//Max (Lowest) priority encountered so far.
  
  //This assert makes sure that there always exist an element in the 
  // Queue when we start the queue traversal. 
  _Assert( !_Processor_mask_Zero( &filter_node->Affinity ) );
  
  cpu_max = _SMP_Get_processor_maximum();
  
  for ( cpu_index = 0 ; cpu_index < cpu_max ; ++cpu_index ) { 
    Struct[ cpu_index ].visited = false;
    
    //Checks if the thread_CPU is in the affinity set of the node
    if ( _Processor_mask_Is_set( &filter_node->Affinity, cpu_index) ) { 
      Per_CPU_Control *cpu = _Per_CPU_Get_by_index( cpu_index );
         
      if ( _Per_CPU_Is_processor_online( cpu ) ) {
        rear = rear + 1;
        Struct[ rear ].Cpu = cpu;
        Struct[ cpu_index ].visited = true;
        Struct[ cpu_index ].caller = filter_base;
      }
    }
  }
  
  while( front <= rear ) {
    curr_CPU = Struct[ front ].Cpu; 
    front = front + 1;
    
    curr_thread = curr_CPU->executing;
    curr_node = _Thread_Scheduler_get_home_node( curr_thread );
  
    curr_priority = _Scheduler_Node_get_priority( curr_node );
    curr_priority = SCHEDULER_PRIORITY_PURIFY( curr_priority ); 
    
    curr_strong_node = _Scheduler_strong_APA_Node_downcast( curr_node );  
      
    if ( curr_priority > max_priority_num ) {
      ret = curr_node;
      max_priority_num = curr_priority;
      
      if( curr_priority > filter_priority )
      {
      	cpu_to_preempt = curr_CPU;
      }
    }
    
    if ( !curr_thread->is_idle ) {
      for ( cpu_index = 0 ; cpu_index < cpu_max ; ++cpu_index ) {
        if ( _Processor_mask_Is_set( &curr_strong_node->Affinity, cpu_index ) ) { 
          //Checks if the thread_CPU is in the affinity set of the node
          Per_CPU_Control *cpu = _Per_CPU_Get_by_index( cpu_index );
          if ( _Per_CPU_Is_processor_online( cpu ) && Struct[ cpu_index ].visited == false ) {
            rear = rear + 1;
            Struct[ rear ].Cpu = cpu;
            Struct[ cpu_index ].visited = true;
            Struct[ cpu_index ].caller = curr_node;
          }
        }  
      }
    }
  }    
  
  if( ret->Priority.value > filter_priority ) {
    //Backtrack on the path from
    //_Thread_Get_CPU(ret->user) to ret, shifting along every task
    
    curr_node = Struct[ _Per_CPU_Get_index(cpu_to_preempt) ].caller;
    curr_CPU = _Thread_Get_CPU( curr_node->user );
    
    //In case the lowest scheduled is on a processor which is directly
    // reachable, there has to be no task shifting.
    if( curr_node != filter_base) {	 
    
      curr_node = Struct[ _Per_CPU_Get_index( curr_CPU ) ].caller;
      curr_CPU = _Thread_Get_CPU( curr_node->user );
	    
      do{     
        next_CPU = _Thread_Get_CPU( curr_node->user );
	 
	_Scheduler_SMP_Preempt(
	  context,
	  curr_node,
	  _Thread_Scheduler_get_home_node( curr_CPU->executing ),
	  _Scheduler_strong_APA_Allocate_processor
	);
		
	curr_CPU = next_CPU;
	curr_node = Struct[ _Per_CPU_Get_index( curr_CPU ) ].caller;
	      
        }while( curr_node != filter_base );
	      
        _Scheduler_SMP_Preempt(
          context,
          curr_node,
          _Thread_Scheduler_get_home_node( curr_CPU->executing ),
          _Scheduler_strong_APA_Allocate_processor
        );
      
        filter_base = Struct[ _Per_CPU_Get_index(cpu_to_preempt) ].caller;
      }
  
  }
  return ret;
}

static inline void _Scheduler_strong_APA_Extract_from_scheduled(
  Scheduler_Context *context,
  Scheduler_Node    *node_to_extract
)
{
  Scheduler_strong_APA_Context *self;
  Scheduler_strong_APA_Node    *node;

  self = _Scheduler_strong_APA_Get_self( context );
  node = _Scheduler_strong_APA_Node_downcast( node_to_extract );

  _Scheduler_SMP_Extract_from_scheduled( &self->Base.Base, &node->Base.Base );
  //Not removing it from All_nodes since the node could go in the ready state.
}

static inline void _Scheduler_strong_APA_Extract_from_ready(
  Scheduler_Context *context,
  Scheduler_Node    *node_to_extract
)
{
  Scheduler_strong_APA_Context *self;
  Scheduler_strong_APA_Node    *node;

  self = _Scheduler_strong_APA_Get_self( context );
  node = _Scheduler_strong_APA_Node_downcast( node_to_extract );
 
  _Assert( !_Chain_Is_empty(self->All_nodes) );
  _Assert( !_Chain_Is_node_off_chain( &node->Node ) );
   
   _Chain_Extract_unprotected( &node->Node );	//Removed from All_nodes
   _Chain_Set_off_chain( &node->Node );
}

static inline void _Scheduler_strong_APA_Insert_ready(
  Scheduler_Context *context,
  Scheduler_Node    *node_base,
  Priority_Control   insert_priority
)
{
  Scheduler_strong_APA_Context *self;
  Scheduler_strong_APA_Node    *node;

  self = _Scheduler_strong_APA_Get_self( context );
  node = _Scheduler_strong_APA_Node_downcast( node_base );
  
  _Assert( !_Chain_Is_node_off_chain( &node->Node ) );
     
  _Chain_Append_unprotected( &self->All_nodes, &node->Node );
}

static inline void _Scheduler_strong_APA_Move_from_scheduled_to_ready(
  Scheduler_Context *context,
  Scheduler_Node    *scheduled_to_ready
)
{
  Priority_Control insert_priority;

  _Scheduler_SMP_Extract_from_scheduled( context, scheduled_to_ready );
  insert_priority = _Scheduler_SMP_Node_priority( scheduled_to_ready );
  
  _Scheduler_strong_APA_Insert_ready(
    context,
    scheduled_to_ready,
    insert_priority
  );
}

static inline void  _Scheduler_strong_APA_Move_from_ready_to_scheduled(
  Scheduler_Context *context,
  Scheduler_Node    *ready_to_scheduled
)
{
  Priority_Control insert_priority;

  _Scheduler_strong_APA_Extract_from_ready( context, ready_to_scheduled );
  insert_priority = _Scheduler_SMP_Node_priority( ready_to_scheduled );
  insert_priority = SCHEDULER_PRIORITY_APPEND( insert_priority );
  _Scheduler_SMP_Insert_scheduled(
    context,
    ready_to_scheduled,
    insert_priority
  );
  //Note: The node still stays in the All_nodes chain
}

static inline bool _Scheduler_strong_APA_Enqueue(
  Scheduler_Context *context,
  Scheduler_Node    *node,
  Priority_Control   insert_priority
)
{//I'm hoping all this works on its own.
  return _Scheduler_SMP_Enqueue(
    context,
    node,
    insert_priority,
    _Scheduler_SMP_Priority_less_equal,
    _Scheduler_strong_APA_Insert_ready,
    _Scheduler_SMP_Insert_scheduled,
    _Scheduler_strong_APA_Move_from_scheduled_to_ready,
    _Scheduler_strong_APA_Get_lowest_scheduled,
    _Scheduler_strong_APA_Allocate_processor
  );
}

static inline bool _Scheduler_strong_APA_Enqueue_scheduled(
  Scheduler_Context *context,
  Scheduler_Node    *node,
  Priority_Control   insert_priority
)
{	
  return _Scheduler_SMP_Enqueue_scheduled(
    context,
    node,
    insert_priority,
    _Scheduler_SMP_Priority_less_equal,
    _Scheduler_strong_APA_Extract_from_ready,
    _Scheduler_strong_APA_Get_highest_ready,
    _Scheduler_strong_APA_Insert_ready,
    _Scheduler_SMP_Insert_scheduled,
    _Scheduler_strong_APA_Move_from_ready_to_scheduled,
    _Scheduler_strong_APA_Allocate_processor
  );
}

static inline bool _Scheduler_strong_APA_Do_ask_for_help(
  Scheduler_Context *context,
  Thread_Control    *the_thread,
  Scheduler_Node    *node
)
{
  return _Scheduler_SMP_Ask_for_help(
    context,
    the_thread,
    node,
    _Scheduler_SMP_Priority_less_equal,
    _Scheduler_strong_APA_Insert_ready,
    _Scheduler_SMP_Insert_scheduled,
    _Scheduler_strong_APA_Move_from_scheduled_to_ready,
    _Scheduler_strong_APA_Get_lowest_scheduled,
    _Scheduler_strong_APA_Allocate_processor
  );
}

static inline void _Scheduler_strong_APA_Register_idle(
  Scheduler_Context *context,
  Scheduler_Node    *idle_base,
  Per_CPU_Control   *cpu
)
{
  (void) context;
  (void) idle_base;
  (void) cpu;
  //We do not maintain a variable to access the scheduled
  //node for a CPU. So this function does nothing.
}

static inline  void  _Scheduler_strong_APA_Do_set_affinity(
  Scheduler_Context *context,
  Scheduler_Node    *node_base,
  void              *arg
)
{
  Scheduler_strong_APA_Node *node;
  const Processor_mask       *affinity;

  node = _Scheduler_strong_APA_Node_downcast( node_base );
  affinity = arg;
  node->Affinity = *affinity;
}

void _Scheduler_strong_APA_Initialize( const Scheduler_Control *scheduler )
{
  Scheduler_strong_APA_Context *self =
    _Scheduler_strong_APA_Get_context( scheduler );

  _Scheduler_SMP_Initialize( &self->Base );
  _Chain_Initialize_empty( &self->All_nodes );
}

void _Scheduler_strong_APA_Yield(
  const Scheduler_Control *scheduler,
  Thread_Control          *thread,
  Scheduler_Node          *node
)
{
  Scheduler_Context *context = _Scheduler_Get_context( scheduler );

  _Scheduler_SMP_Yield(
    context,
    thread,
    node,
    _Scheduler_strong_APA_Extract_from_ready,
    _Scheduler_strong_APA_Enqueue,
    _Scheduler_strong_APA_Enqueue_scheduled
  );
}

void _Scheduler_strong_APA_Block(
  const Scheduler_Control *scheduler,
  Thread_Control          *thread,
  Scheduler_Node          *node
)
{
  Scheduler_Context *context = _Scheduler_Get_context( scheduler );
//The extract from ready automatically removes the node from All_nodes chain.
  _Scheduler_SMP_Block(
    context,
    thread,
    node,
    _Scheduler_strong_APA_Extract_from_scheduled,
    _Scheduler_strong_APA_Extract_from_ready,
    _Scheduler_strong_APA_Get_highest_ready,
    _Scheduler_strong_APA_Move_from_ready_to_scheduled,
    _Scheduler_strong_APA_Allocate_processor
  );
}

void _Scheduler_strong_APA_Unblock(
  const Scheduler_Control *scheduler,
  Thread_Control          *thread,
  Scheduler_Node          *node
)
{
  Scheduler_Context *context = _Scheduler_Get_context( scheduler );

  _Scheduler_SMP_Unblock(
    context,
    thread,
    node,
    _Scheduler_strong_APA_Do_update,
    _Scheduler_strong_APA_Enqueue
  );
}

void _Scheduler_strong_APA_Update_priority(
  const Scheduler_Control *scheduler,
  Thread_Control          *thread,
  Scheduler_Node          *node
)
{
  Scheduler_Context *context = _Scheduler_Get_context( scheduler );

  _Scheduler_SMP_Update_priority(
    context,
    thread,
    node,
    _Scheduler_strong_APA_Extract_from_ready,
    _Scheduler_strong_APA_Do_update,
    _Scheduler_strong_APA_Enqueue,
    _Scheduler_strong_APA_Enqueue_scheduled,
    _Scheduler_strong_APA_Do_ask_for_help
  );
}

bool _Scheduler_strong_APA_Ask_for_help(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  Scheduler_Context *context = _Scheduler_Get_context( scheduler );

  return _Scheduler_strong_APA_Do_ask_for_help(
    context,
    the_thread,
    node
  );
}

void _Scheduler_strong_APA_Reconsider_help_request(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  Scheduler_Context *context = _Scheduler_Get_context( scheduler );

  _Scheduler_SMP_Reconsider_help_request(
    context,
    the_thread,
    node,
    _Scheduler_strong_APA_Extract_from_ready
  );
}

void _Scheduler_strong_APA_Withdraw_node(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node,
  Thread_Scheduler_state   next_state
)
{
  Scheduler_Context *context = _Scheduler_Get_context( scheduler );

  _Scheduler_SMP_Withdraw_node(
    context,
    the_thread,
    node,
    next_state,
    _Scheduler_strong_APA_Extract_from_ready,
    _Scheduler_strong_APA_Get_highest_ready,
    _Scheduler_strong_APA_Move_from_ready_to_scheduled,
    _Scheduler_strong_APA_Allocate_processor
  );
}

void _Scheduler_strong_APA_Add_processor(
  const Scheduler_Control *scheduler,
  Thread_Control          *idle
)
{
  Scheduler_Context *context = _Scheduler_Get_context( scheduler );

  _Scheduler_SMP_Add_processor(
    context,
    idle,
    _Scheduler_strong_APA_Has_ready,
    _Scheduler_strong_APA_Enqueue_scheduled,
    _Scheduler_strong_APA_Register_idle
  );
}

Thread_Control *_Scheduler_strong_APA_Remove_processor(
  const Scheduler_Control *scheduler,
  Per_CPU_Control         *cpu
)
{
  Scheduler_Context *context = _Scheduler_Get_context( scheduler );

  return _Scheduler_SMP_Remove_processor(
    context,
    cpu,
    _Scheduler_strong_APA_Extract_from_ready,
    _Scheduler_strong_APA_Enqueue
  );
}

void _Scheduler_strong_APA_Node_initialize(
  const Scheduler_Control *scheduler,
  Scheduler_Node          *node,
  Thread_Control          *the_thread,
  Priority_Control         priority
)
{
  Scheduler_SMP_Node *smp_node;
  Scheduler_strong_APA_Node *strong_node;
  
  smp_node = _Scheduler_SMP_Node_downcast( node );  
  strong_node = _Scheduler_strong_APA_Node_downcast( node );
  
  _Scheduler_SMP_Node_initialize( scheduler, smp_node, the_thread, priority );
  _Processor_mask_Assign(
    &strong_node->Affinity,
   _SMP_Get_online_processors()
  );
}

void _Scheduler_strong_APA_Start_idle(
  const Scheduler_Control *scheduler,
  Thread_Control          *idle,
  Per_CPU_Control         *cpu
)
{
  Scheduler_Context *context;

  context = _Scheduler_Get_context( scheduler );

  _Scheduler_SMP_Do_start_idle(
    context,
    idle,
    cpu,
    _Scheduler_strong_APA_Register_idle
  );
}

bool _Scheduler_strong_APA_Set_affinity(
  const Scheduler_Control *scheduler,
  Thread_Control          *thread,
  Scheduler_Node          *node_base,
  const Processor_mask    *affinity
)
{
  Scheduler_Context         *context;
  Scheduler_strong_APA_Node *node;
  Processor_mask             local_affinity;
 
  context = _Scheduler_Get_context( scheduler );
  _Processor_mask_And( &local_affinity, &context->Processors, affinity );

  if ( _Processor_mask_Is_zero( &local_affinity ) ) {
    return false;
  }

  node = _Scheduler_strong_APA_Node_downcast( node_base );

  if ( _Processor_mask_Is_equal( &node->Affinity, affinity ) )
    return true;	//Nothing to do. Return true.

 _Processor_mask_Assign( &node->Affinity, &local_affinity );

 _Scheduler_SMP_Set_affinity(
   context,
   thread,
   node_base,
   &local_affinity,
   _Scheduler_strong_APA_Do_set_affinity,
   _Scheduler_strong_APA_Extract_from_ready,		
   _Scheduler_strong_APA_Get_highest_ready,	
   _Scheduler_strong_APA_Move_from_ready_to_scheduled,
   _Scheduler_strong_APA_Enqueue,
   _Scheduler_strong_APA_Allocate_processor
 );

  return true;
}

