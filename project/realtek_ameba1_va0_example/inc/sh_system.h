/*
 * Copyright (C) 2018 SHARP Corporation. All rights reserved.
 */

#ifndef SH_INCLUDE_SH_SYSTEM_H_
#define SH_INCLUDE_SH_SYSTEM_H_

#if (CONFIG_AUDREY_EXT_INFO == 1)
	/*************************************************************************************************************************
	 * 定数宣言
	 *******************************/
	#define SH_SYS_TASK_NAME_SIZE_MAX (10)

	/*************************************************************************************************************************
	 * 型宣言
	 *******************************/

	typedef struct sh_sys_task_info{
		uint32_t id;								//!< Task ID
		char 	*name[SH_SYS_TASK_NAME_SIZE_MAX+1];	//!< task name
		uint32_t stack_free_on_hwm;					//!< stack free size on High Water mark (スタック最高使用時のスタック残量バイト数)
		uint32_t stack_size;						//!< stack size in byte
		uint32_t priority;							//!< task priority (0:優先度最低)
	}sh_sys_task_info_t;

	typedef struct sh_sys_all_tasks_info
	{
		uint16_t task_count;	//tasks_infoの配列の要素数
		sh_sys_task_info_t	tasks_info[1];
	}sh_sys_all_tasks_info_t;

	typedef void* sh_sys_thread_t;

	/*************************************************************************************************************************
	 * Task list 関連
	 *******************************/

	sh_sys_all_tasks_info_t* sh_sys_task_get_info();


	int sh_sys_stdout_enable(int enable);
	void sh_sys_stdout_set_writter(void (*stdio_writer_function)( const char* str, unsigned long len ));
	void (*sh_sys_stdout_get_writter())( const char*, unsigned long);

	void sh_sys_exec_without_taskswitch(void (*task)(void*), void *pUserPtr);

	#endif /* SH_INCLUDE_SH_SYSTEM_H_ */
#endif

void sh_sys_force_hardfault(int);
void vApplicationMallocFailedHook(void);
