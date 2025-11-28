#ifndef TASK_TEST_H
#define TASK_TEST_H

/**
 * Task Manager Test Module
 * 
 * This module provides comprehensive testing for the task manager
 * system including task creation, scheduling, priority handling,
 * and context switching.
 */

/**
 * Run comprehensive task manager tests
 * 
 * This function will:
 * - Initialize the task manager
 * - Create multiple test tasks with different priorities
 * - Exercise task scheduling and context switching
 * - Verify priority-based scheduling behavior
 * - Collect and report performance statistics
 */
void task_manager_test(void);

#endif /* TASK_TEST_H */