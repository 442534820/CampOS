#include "campos.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>


/*******************/
static uint16_t current_task_id;
static uint32_t current_time;
static campos_tcb_t campos_task_list[CAMPOS_CONFIG_TASK_MAX_NUM];
static campos_task_handle_t campos_task_index = 0;
static campos_msg_t msg_pool[CAMPOS_CONFIG_MSG_POOL_NUM];
static campos_timer_t timer_pool[CAMPOS_CONFIG_TIMER_POOL_NUM];

/**********************************************
 * Link List
 */
typedef struct campos_list_t
{
	struct campos_list_t *next;
	void* data;
}campos_list_t;

campos_list_t* campos_list_add_to_tail(campos_list_t *list, campos_list_t *item)
{
	campos_list_t *p;

	for(p = list; p->next; p = p->next){}
	p->next = item;
	return list;
}

campos_list_t* campos_list_add_to_head(campos_list_t *list, campos_list_t *item)
{
	item->next = list;
	return item;
}

campos_list_t* campos_list_add(campos_list_t *list, campos_list_t *item)
{
	return campos_list_add_to_tail(list, item);
}

/**********************************************
 * Queue
 */
typedef struct campos_queue_t
{
	void* data_buf;
	uint16_t size;
	uint16_t count;
	void** read_ptr;
	void** write_ptr;
}campos_queue_t;

int campos_queue_create(campos_queue_t *queue, void* data_buf, uint16_t data_count)
{
	if(queue == NULL)
		return -1;

	queue->size = data_count * sizeof(void*);
	queue->data_buf = data_buf;
	queue->count = 0;
	queue->read_ptr = queue->data_buf;
	queue->write_ptr = queue->data_buf;

	return 0;
}

int campos_queue_in(campos_queue_t *queue, void* data)
{
	if(queue->count == queue->size)
		return -1;

	*queue->write_ptr = data;
	queue->write_ptr++;
	if(queue->write_ptr == (void**)((size_t)(queue->data_buf) + queue->size))
		queue->write_ptr = queue->data_buf;

	return 0;
}

int campos_queue_out(campos_queue_t *queue, void** data)
{
	if(queue->count == 0)
		return -1;

	if(data)
		*data = *queue->read_ptr;
	queue->read_ptr++;
	if(queue->read_ptr == (void**)((size_t)(queue->data_buf) + queue->size))
		queue->read_ptr = queue->data_buf;

	return 0;
}

/**********************************************
 * Message
 */

int campos_msg_send(campos_task_handle_t task_handle, campos_msg_id_t msg_id, uint32_t param)
{
	int i;
	campos_msg_t *pmsg;
	
	if(task_handle > campos_task_index)
		return -1;  // No task
	if(msg_id == CAMPOS_MSG_NO_MESSAGE)
		return -1;  // Error message
	// Alloc a free msg struct
	for(i=0 ;i<CAMPOS_CONFIG_MSG_POOL_NUM; i++)
	{
		if(msg_pool[i].msg_id == CAMPOS_MSG_NO_MESSAGE)
			break;
	}
	if(i == CAMPOS_CONFIG_MSG_POOL_NUM)
		return -1;  // Alloc fail
	// Message in queue
	msg_pool[i].msg_id = msg_id;
	msg_pool[i].param = param;
	if(campos_task_list[task_handle].message == NULL)
	{
		campos_task_list[task_handle].message = &msg_pool[i];
	}
	else
	{
		pmsg = campos_task_list[task_handle].message;
		while(pmsg->next != NULL)
			pmsg = pmsg->next;
		pmsg->next = &msg_pool[i];
	}

	return 0;
}

int campos_msg_recv(campos_task_handle_t task_handle, campos_msg_id_t *msg_id, uint32_t *param)
{
	campos_msg_t *pmsg;
	
	if(task_handle > campos_task_index)
		return -1;  // No task
	pmsg = campos_task_list[task_handle].message;  // fetch 1st messge
	if(pmsg == NULL)
		return -1;  // No message
	if(msg_id != NULL)
		*msg_id = pmsg->msg_id;
	if(param != NULL)
		*param = pmsg->param;
	pmsg->msg_id = CAMPOS_MSG_NO_MESSAGE; // link down
	campos_task_list[task_handle].message = pmsg->next;

	return 0;
}


int campos_timer_init(campos_msg_id_t msg_id, uint32_t period)
{
	int i;
	
	for(i=0; i<CAMPOS_CONFIG_TIMER_POOL_NUM; i++)
	{
		if(timer_pool[i].msg_id == CAMPOS_MSG_NO_MESSAGE)
			break;
	}
	if(i == CAMPOS_CONFIG_TIMER_POOL_NUM)
		return -1;
	timer_pool[i].task_handle = current_task_id;
	timer_pool[i].msg_id = msg_id;
	timer_pool[i].period = period;
	timer_pool[i].next_time_stamp = current_time + period;
	
	return 0;
}

int campos_timer_free(void)
{
	timer_pool[current_task_id].msg_id = CAMPOS_MSG_NO_MESSAGE;
	
	return 0;
}

void campos_tick_handler(void)
{
	int i;
	
	current_time++;
	for(i=0; i<CAMPOS_CONFIG_TIMER_POOL_NUM; i++)
	{
		if(timer_pool[i].msg_id != CAMPOS_MSG_NO_MESSAGE)
		{
			if((int32_t)(timer_pool[i].next_time_stamp - current_time) <= 0)
			{
				timer_pool[i].next_time_stamp += timer_pool[i].period;
				campos_msg_send(timer_pool[i].task_handle, timer_pool[i].msg_id, 0);
			}
		}
	}
}

/**********************************************
 * Task
 */
campos_task_handle_t campos_task_init(campos_task_func_t *task_function, const char* task_name)
{
#if CAMPOS_CONFIG_TASK_NAME_SUPPORT
	strncpy(campos_task_list[campos_task_index].task_name, task_name, CAMPOS_CONFIG_TASK_NAME_MAX_LEN);
#endif
	campos_task_list[campos_task_index].task_function = task_function;
	campos_task_list[campos_task_index].message = NULL;
	campos_task_list[campos_task_index].state = CAMPOS_TASK_STATE_RUN;
	campos_msg_send(campos_task_index, CAMPOS_MSG_TASK_INIT, 0);
	campos_task_index++;

	return campos_task_index - 1;
}

void campos_run(void)
{
	campos_tcb_t* ptcb;
	int ret_val;
	campos_msg_id_t msg_id;
	uint32_t msg_param;
	int i;
	
	for(;;)
	{
		for(i=0; i<campos_task_index; i++)
		{
			ptcb = &campos_task_list[i];
			if(ptcb->state == CAMPOS_TASK_STATE_RUN)
			{
				if(ptcb->message != NULL)
				{
					ret_val = campos_msg_recv(i, &msg_id, &msg_param);
					current_task_id = i;
					ret_val = ptcb->task_function(msg_id, msg_param);
				}
			}
		}
	}
}
