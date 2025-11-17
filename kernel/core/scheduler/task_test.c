#include "task_manager.h"
#include "../kernel.h"
#include "config.h"

/* Test task functions */
static int test_task_1(void *data);
static int test_task_2(void *data);
static int test_task_3(void *data);

/* Test data */
static volatile int test_counter = 0;
static volatile bool test_complete = false;

/**
 * Test the task manager functionality
 */
void task_manager_test(void)
{
    SERIAL_LOG("TASK_TEST: Starting task manager tests\n");
    
    /* Initialize task manager */
    task_manager_init();
    
    /* Create test tasks */
    task_t *task1 = task_create("test1", test_task_1, (void*)1, 
                                TASK_PRIORITY_HIGH, TASK_FLAG_PREEMPTIBLE);
    task_t *task2 = task_create("test2", test_task_2, (void*)2, 
                                TASK_PRIORITY_NORMAL, TASK_FLAG_PREEMPTIBLE);
    task_t *task3 = task_create("test3", test_task_3, (void*)3, 
                                TASK_PRIORITY_LOW, TASK_FLAG_PREEMPTIBLE);
    
    if (!task1 || !task2 || !task3) {
        SERIAL_LOG("TASK_TEST: ERROR - Failed to create test tasks\n");
        return;
    }
    
    SERIAL_LOG("TASK_TEST: Created test tasks\n");
    
    /* Start the tasks */
    task_start(task1);
    task_start(task2);  
    task_start(task3);
    
    SERIAL_LOG("TASK_TEST: Started test tasks\n");
    
    /* Let tasks run for a while */
    for (int i = 0; i < 100 && !test_complete; i++) {
        task_schedule();
        task_sleep(10);  /* 10ms delay */
    }
    
    /* Print statistics */
    task_manager_stats_t stats;
    task_get_stats(&stats);
    
    SERIAL_LOG("TASK_TEST: Final statistics:\n");
    SERIAL_LOG("  Total tasks: ");
    SERIAL_LOG_HEX("", stats.total_tasks);
    SERIAL_LOG("\n");
    SERIAL_LOG("  Active tasks: ");
    SERIAL_LOG_HEX("", stats.active_tasks);
    SERIAL_LOG("\n");
    SERIAL_LOG("  Context switches: ");
    SERIAL_LOG_HEX("", stats.context_switches);
    SERIAL_LOG("\n");
    SERIAL_LOG("  Scheduler calls: ");
    SERIAL_LOG_HEX("", stats.scheduler_calls);
    SERIAL_LOG("\n");
    SERIAL_LOG("  Test counter: ");
    SERIAL_LOG_HEX("", test_counter);
    SERIAL_LOG("\n");
    
    /* Terminate test tasks */
    task_terminate(task1);
    task_terminate(task2);
    task_terminate(task3);
    
    SERIAL_LOG("TASK_TEST: Test completed\n");
}

/**
 * Test task 1 - High priority
 */
static int test_task_1(void *data)
{
    int task_num = (int)data;
    SERIAL_LOG("TASK_TEST: Task ");
    SERIAL_LOG_HEX("", task_num);
    SERIAL_LOG(" (HIGH) started\n");
    
    for (int i = 0; i < 5; i++) {
        SERIAL_LOG("TASK_TEST: Task ");
        SERIAL_LOG_HEX("", task_num);
        SERIAL_LOG(" iteration ");
        SERIAL_LOG_HEX("", i);
        SERIAL_LOG("\n");
        
        test_counter++;
        task_yield();
        task_sleep(20);
    }
    
    SERIAL_LOG("TASK_TEST: Task ");
    SERIAL_LOG_HEX("", task_num);
    SERIAL_LOG(" (HIGH) completed\n");
    
    return 0;
}

/**
 * Test task 2 - Normal priority
 */
static int test_task_2(void *data)
{
    int task_num = (int)data;
    SERIAL_LOG("TASK_TEST: Task ");
    SERIAL_LOG_HEX("", task_num);
    SERIAL_LOG(" (NORMAL) started\n");
    
    for (int i = 0; i < 3; i++) {
        SERIAL_LOG("TASK_TEST: Task ");
        SERIAL_LOG_HEX("", task_num);
        SERIAL_LOG(" iteration ");
        SERIAL_LOG_HEX("", i);
        SERIAL_LOG("\n");
        
        test_counter += 2;
        task_yield();
        task_sleep(30);
    }
    
    SERIAL_LOG("TASK_TEST: Task ");
    SERIAL_LOG_HEX("", task_num);
    SERIAL_LOG(" (NORMAL) completed\n");
    
    return 0;
}

/**
 * Test task 3 - Low priority
 */
static int test_task_3(void *data)
{
    int task_num = (int)data;
    SERIAL_LOG("TASK_TEST: Task ");
    SERIAL_LOG_HEX("", task_num);
    SERIAL_LOG(" (LOW) started\n");
    
    for (int i = 0; i < 2; i++) {
        SERIAL_LOG("TASK_TEST: Task ");
        SERIAL_LOG_HEX("", task_num);
        SERIAL_LOG(" iteration ");
        SERIAL_LOG_HEX("", i);
        SERIAL_LOG("\n");
        
        test_counter += 3;
        task_yield();
        task_sleep(50);
    }
    
    SERIAL_LOG("TASK_TEST: Task ");
    SERIAL_LOG_HEX("", task_num);
    SERIAL_LOG(" (LOW) completed\n");
    
    test_complete = true;
    return 0;
}