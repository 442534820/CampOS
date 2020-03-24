#ifndef __CAMPOS_H__
#define __CAMPOS_H__

#include <stdint.h>

/*
 * Camp OS (Cloud Abstract Message Process Operating System)
 */

#define CAMPOS_CONFIG_TASK_NAME_SUPPORT   1
#define CAMPOS_CONFIG_TASK_NAME_MAX_LEN   16
#define CAMPOS_CONFIG_TASK_MAX_NUM        10
#define CAMPOS_CONFIG_MSG_POOL_NUM        16
#define CAMPOS_CONFIG_TIMER_POOL_NUM      4


typedef enum
{
	CAMPOS_MSG_NO_MESSAGE = 0x0,
	CAMPOS_MSG_TASK_INIT = 0x1,
	CAMPOS_MSG_TIMER_TRIG,
	CAMPOS_MSG_CUSTOM_START = 0x40000000,
}campos_msg_id_t;

typedef int campos_task_handle_t;

typedef struct campos_msg_t
{
	campos_msg_id_t msg_id;
	uint32_t param;
	struct campos_msg_t* next;
}campos_msg_t;

typedef enum
{
	CAMPOS_TASK_STATE_IDLE = 0x0,
	CAMPOS_TASK_STATE_RUN,
	CAMPOS_TASK_STATE_SLEEP,
	CAMPOS_TASK_STATE_DONE
}campos_task_state_t;

typedef int(campos_task_func_t)(campos_msg_id_t msg_id, uint32_t param);

typedef struct campos_tcb_t
{
	campos_task_state_t state;
	campos_task_func_t *task_function;
#if CAMPOS_CONFIG_TASK_NAME_SUPPORT
	char task_name[CAMPOS_CONFIG_TASK_NAME_MAX_LEN];
#endif
	campos_msg_t* message;
}campos_tcb_t;

typedef struct campos_timer_t
{
	uint32_t next_time_stamp;
	uint32_t period;
	campos_msg_id_t msg_id;
	campos_task_handle_t task_handle;
}campos_timer_t;

void campos_run(void);
campos_task_handle_t campos_task_init(campos_task_func_t *task_function, const char* task_name);
int campos_msg_send(campos_task_handle_t task_handle, campos_msg_id_t msg_id, uint32_t param);
int campos_timer_init(campos_msg_id_t msg_id, uint32_t period);
void campos_tick_handler(void);
 
#endif //__CAMPOS_H__
