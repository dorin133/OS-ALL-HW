#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/sched.h>

asmlinkage long sys_hello(void) {
	printk("Hello, World!\n");
	return 0;
}

asmlinkage long sys_set_weight(int weight){
  if(weight < 0){
    return -EINVAL;
  }
  current->weight = weight;
  return 0;
}

asmlinkage long sys_get_weight(void){
  return current->weight;
}

int calculate_sum_of_children(struct task_struct *current_task){
  struct task_struct *current_task_in_list;
  struct list_head *list;
  int sum = current_task->weight;
  list_for_each(list, &current_task->children){

    current_task_in_list = list_entry(list, struct task_struct, sibling);
    sum = sum + calculate_sum_of_children(current_task_in_list);
  }
  return sum;
}


asmlinkage long sys_get_children_sum(void){
  if(!list_empty(&current->children)) {
	return calculate_sum_of_children(current) - (current->weight) ;
  }
  return -ECHILD;
}


asmlinkage long sys_get_heaviest_ancestor(void) {
  pid_t max_pid;
  int max;
  struct task_struct *task, *prev;
  task = current;
  max = current->weight;
  do {
      prev = task;
      if(max < task->weight){ 
        max = task->weight;
        max_pid = task->pid;
      }
      task = task->parent;
  } while (prev->pid != 1);
  return max_pid;
}





