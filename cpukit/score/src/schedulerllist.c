/**
 * @file
 *
 * @brief Scheduler Llist Functions
 *
 * @ingroup RTEMSScoreScheduler
 */

/*
 *
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/score/schedulerllistimpl.h>
#include <rtems/score/chainimpl.h>
#include <rtems/score/thread.h>


void _Scheduler_Llist_Initialize( const Scheduler_Control *scheduler )
{
  Scheduler_Llist_Context *context =
    _Scheduler_Llist_Get_context( scheduler );

  _Chain_Initialize_empty( &context->Ready );
}


void _Scheduler_Llist_Block(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  _Scheduler_Generic_block(
    scheduler,
    the_thread,
    node,
    _Scheduler_Llist_Extract,
    _Scheduler_Llist_Schedule_body
  );
}


void _Scheduler_Llist_Yield(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  Scheduler_Llist_Context *context;
  unsigned int              insert_priority;

  context = _Scheduler_Llist_Get_context( scheduler );

  (void) node;

  _Chain_Extract_unprotected( &the_thread->Object.Node );
  insert_priority = (unsigned int) _Thread_Get_priority( the_thread );
  insert_priority = SCHEDULER_PRIORITY_APPEND( insert_priority );
  _Scheduler_Llist_Insert( &context->Ready, the_thread, insert_priority );
  _Scheduler_Llist_Schedule_body( scheduler, the_thread, false );
}


void _Scheduler_Llist_Unblock(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  Scheduler_Llist_Context *context;
  unsigned int              priority;
  unsigned int              insert_priority;

  (void) node;

  context = _Scheduler_Llist_Get_context( scheduler );
  priority = _Thread_Get_priority( the_thread );
  insert_priority = SCHEDULER_PRIORITY_APPEND( priority );
  _Scheduler_Llist_Insert( &context->Ready, the_thread, insert_priority );

  /*
   *  If the thread that was unblocked is more important than the heir,
   *  then we have a new heir.  This may or may not result in a
   *  context switch.
   *
   *  Normal case:
   *    If the current thread is preemptible, then we need to do
   *    a context switch.
   *  Pseudo-ISR case:
   *    Even if the thread isn't preemptible, if the new heir is
   *    a pseudo-ISR system task, we need to do a context switch.
   */
  if ( priority < _Thread_Get_priority( _Thread_Heir ) ) {
    _Scheduler_Update_heir(
      the_thread,
      priority == PRIORITY_PSEUDO_ISR
    );
  }
}


void _Scheduler_Llist_Schedule(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread
)
{
  _Scheduler_Llist_Schedule_body( scheduler, the_thread, false );
}

void _Scheduler_Llist_Update_priority(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  Scheduler_Llist_Context *context;
  unsigned int              new_priority;

  if ( !_Thread_Is_ready( the_thread ) ) {
    /* Nothing to do */
    return;
  }

  context = _Scheduler_Llist_Get_context( scheduler );
  new_priority = (unsigned int ) _Scheduler_Node_get_priority( node );

  _Scheduler_Llist_Extract( scheduler, the_thread, node );
  _Scheduler_Llist_Insert( &context->Ready, the_thread, new_priority );
  _Scheduler_Llist_Schedule_body( scheduler, the_thread, false );
}




void _Scheduler_Llist_Update_priority(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  Scheduler_Llist_Context *context;
  unsigned int              new_priority;

  if ( !_Thread_Is_ready( the_thread ) ) {
    /* Nothing to do */
    return;
  }

  context = _Scheduler_Llist_Get_context( scheduler );
  new_priority = (unsigned int ) _Scheduler_Node_get_priority( node );

  _Scheduler_Llist_Extract( scheduler, the_thread, node );
  _Scheduler_Llist_Insert( &context->Ready, the_thread, new_priority );
  _Scheduler_Llist_Schedule_body( scheduler, the_thread, false );
}


