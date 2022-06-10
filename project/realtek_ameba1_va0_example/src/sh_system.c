/*
 * Copyright (C) 2018 SHARP Corporation. All rights reserved.
 */
#include <FreeRTOS.h>
#include <task.h>
#include <sys_api.h>
#include <platform_opts.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <state_manager.h>

#if (CONFIG_AUDREY_EXT_INFO == 1)
	#include <sh_system.h>

	/*************************************************************************************************************************
	 * Task list 関連
	 *******************************/

	/*******************************
	 * FreeRTOS hook function
	 *******************************/
	typedef struct TaskList{
		TaskHandle_t 		handle; 	//タスクハンドル
		unsigned long		id;			//タスクID
		size_t				stack_size;	//スタックサイズ(in byte)
		struct TaskList		*next;		//次のノード
	}TaskList_t;
	static TaskList_t *task_list_top = NULL;	//タスクノードチェインの先頭
	static uint32_t		task_list_count = 0; 	//タスクノードの個数

	/*
	 * タスク起動時にタスクノードチェインに追加する
	 * */
	void sh_sys_freertos_task_create_hook(TaskHandle_t pxNewTCB, unsigned long taskNumber, size_t stack_size)
	{
		TaskList_t *task_node = malloc(sizeof(TaskList_t));
		if(task_node==NULL) return;
		memset(task_node, 0, sizeof(TaskList_t));
		task_node->handle = pxNewTCB;
		task_node->id = taskNumber;
		task_node->stack_size = stack_size;

		TaskList_t **insert_point;
		//挿入場所を見つける
		for(insert_point = &task_list_top;
			(*insert_point)!=NULL;
			insert_point = &((*insert_point)->next));

		//追加
		(*insert_point) = task_node;
		task_list_count++;
	}

	/*
	 * タスク終了時にタスクノードチェインから削除する
	 * */
	void sh_sys_freertos_task_delete_hook(TaskHandle_t pxTaskToDelete)
	{
		TaskList_t **target_node,*delete_node;
		//タスクノードを見つける
		for(target_node = &task_list_top;
			(*target_node)!=NULL && (*target_node)->handle != pxTaskToDelete;
			target_node = &((*target_node)->next));
		if((*target_node)==NULL) return; //おかしい、、、。そんなはずは、、、。

		//削除
		delete_node = (*target_node);
		(*target_node) = (*target_node)->next;
		free(delete_node);
		task_list_count--;
	}

	TaskHandle_t	 ___crntTask = NULL;
	const char		*___crntTaskName = NULL;
	void			*___crntTaskTopOfStack = NULL;
	void			*___crntTaskStackArea = NULL;

	void sh_sys_freertos_task_switch_in(TaskHandle_t switchInTask, const char *taskName, void *TopOfStack, void *StackArea)
	{
		___crntTask = switchInTask;
		___crntTaskName = taskName;
		___crntTaskTopOfStack = TopOfStack;
		___crntTaskStackArea = StackArea;
	}

	uint32_t sh_sys_freertos_get_crntTask_StackSize_fromISR()
	{
		TaskList_t *task;
		for(task = task_list_top;
			task!=NULL;
			task = task->next )
		{
			if(task->handle==___crntTask)
			{
				return task->stack_size;
			}
		}
		return 0;
	}

	int sh_sys_rtos_thread_is_alive(sh_sys_thread_t *thread)
	{
		int ret = 0;
		if(thread==NULL) return 0;
		if(*thread==NULL) return 1; //自スレッドの場合はまだ生きている判定

		vTaskSuspendAll();
		{
			TaskList_t *task;
			for(task = task_list_top;
				task!=NULL;
				task = task->next )
			{
				if(task->handle == *thread)
				{
					ret = 1;
					break;
				}
			}
		}
		xTaskResumeAll();
	    return ret;
	}

	/*!
	 * タスク情報を取得する
	 * @note	取得したsh_sys_mem_all_tasks_info_tは使用後呼び出し元でfreeすること
	 * */
	sh_sys_all_tasks_info_t* sh_sys_task_get_info()
	{
		sh_sys_all_tasks_info_t *info;
		vTaskSuspendAll();
		{
			unsigned int idx,taskNum;
			char *taskName;
			TaskList_t *task;

			taskNum = task_list_count;
			info = (sh_sys_all_tasks_info_t *)malloc(sizeof(sh_sys_all_tasks_info_t)+sizeof(sh_sys_task_info_t)*taskNum);
			if (info == NULL)
			{
				return NULL;
			}

			memset(info, 0, sizeof(sh_sys_all_tasks_info_t)+sizeof(sh_sys_task_info_t)*taskNum);
			info->task_count = taskNum;
			for(task = task_list_top,idx=0;
				task!=NULL;
				task = task->next )
			{
				taskName = (char *)pcTaskGetTaskName(task->handle);
				info->tasks_info[idx].id = task->id;
				if(taskName!=NULL) memcpy(info->tasks_info[idx].name, taskName, configMAX_TASK_NAME_LEN);
				info->tasks_info[idx].name[SH_SYS_TASK_NAME_SIZE_MAX]='\0';
				info->tasks_info[idx].stack_size = task->stack_size;
				info->tasks_info[idx].stack_free_on_hwm = uxTaskGetStackHighWaterMark(task->handle) * sizeof(portSTACK_TYPE);
				info->tasks_info[idx].priority = uxTaskPriorityGet(task->handle);
				idx++;
			}
		}
		xTaskResumeAll();
	    return info;
	}


	/*************************************************************************************************************************
	 * 標準出力 関連
	 *******************************/
	/*!
	 * 標準出力の有効・無効を切り替える
	 * @return	切り替え前の状態
	 * */
	int sh_sys_stdout_enable(int enable)
	{
		extern int __enable_stdout(int enable);
		int old = __enable_stdout(enable? 1: 0);
		return old==0? 0: 1;
	}

	/*!
	 * 標準出力の関数を設定する
	 * */
	void sh_sys_stdout_set_writer(
			void (*stdio_writer_function)( const char* str, unsigned long len )	//!< 標準出力を処理する関数のポインタを指定する
			)
	{
		extern void __set_stdout_writer(void (*stdio_writer_function)( const char* str, unsigned long len ) );
		__set_stdout_writer(stdio_writer_function);
	}

	/*************************************************************************************************************************
	 * Utility 関連
	 *******************************/

	/*!
	 * 指定された関数をタスクスイッチを抑制した状態で実行する (ココで指定する関数内でprintfなど、標準出力関数をじっこうしたらダメ。ハングアップします。)
	 * */
	void sh_sys_exec_without_taskswitch(void (*task)(void*), void *pUserPtr)
	{
		vTaskSuspendAll();
		{
			task(pUserPtr);
		}
		( void ) xTaskResumeAll();
	}
	
#endif


/*!
 * 強制的にHardFaultを発生させる
 * */
void sh_sys_force_hardfault(int number)
{
	void (*bad_function)(void)=(void(*)())0xF0000000;
	bad_function();
}

/*!
 * pvPortMallocのフック関数
 * */
void vApplicationMallocFailedHook (void)
{
	SendMessageToStateManager(MSG_MALLOC_FAIL_RESET, PARAM_NONE);	/* ヒープ確保失敗通知 */
//	sh_sys_force_hardfault(3);
}
