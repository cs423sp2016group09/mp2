#define LINUX
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h> 
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/cache.h>
#include <linux/kthread.h>
#include <linux/slab.h>
// #include <linux/mutex.h>
#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_09");
MODULE_DESCRIPTION("CS-423 MP2");

#define DEBUG 1
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;
static struct kmem_cache *cache;
static DEFINE_SPINLOCK(my_lock);

#define RUNNING 0
#define READY 1
#define SLEEPING 2

#define FILENAME "status"
#define DIRECTORY "mp2"
#define READ_BUFFER_SIZE 400
#define LINE_LENGTH 80

static LIST_HEAD(head);
static DEFINE_MUTEX(mutex_list);


typedef struct mp2_task_struct {
    struct task_struct *task;
    struct list_head task_node;
    struct timer_list task_timer;

    unsigned long deadline;

    unsigned int state;

    unsigned int pid;
    unsigned long cputime;
    unsigned long period;

} mp2_struct;

static mp2_struct dispatch_thread;
static mp2_struct* currently_running_task;

static LIST_HEAD(head_task);
static int finished_writing;

static mp2_struct *find_shortest_period(void) {
    mp2_struct *i;

    mp2_struct *highest_priority_ready_task;
    highest_priority_ready_task = NULL;
    
    mutex_lock(&mutex_list);
    // find in the list
    list_for_each_entry(i, &head_task, task_node) {
        // the task with READY state
        if (i->state == READY) {
            if (highest_priority_ready_task == NULL) {
                highest_priority_ready_task = i;
            } else {
                // that has the highest priority (lowest period)
                if (i->period < highest_priority_ready_task->period) {
                    highest_priority_ready_task = i;
                }
            }
        }
    }
    mutex_unlock(&mutex_list);
    return highest_priority_ready_task;
}

static int dispatch_thread_function(void *arg){
    while(1) { // woken up
        printk(KERN_ALERT "WOKEN UP\n");
        
        mp2_struct *next_task;
        
        struct sched_param sparam;

        
        printk(KERN_ALERT "finding shortest\n");


        next_task = find_shortest_period();


        if (next_task == NULL) { // no shortest period
            printk(KERN_ALERT "DID NOT FIND ONE WITH THE SHORTEST PERIOD");
            // no new task in the ready state,   
            // pre-empt the currently running task
            if (currently_running_task != NULL) {
                sparam.sched_priority=0;
                sched_setscheduler(currently_running_task->task, SCHED_NORMAL, &sparam);
            }
            // not sure if we should make it ready or running, since it's not context switching
            // currently_running_task->state = RUNNING;
        } else {
            printk(KERN_ALERT "Will be switching to PID: %u", next_task->pid);
            // there was a currently running task and task has not ended
            if (currently_running_task != NULL && currently_running_task->task != NULL) {
                // pre-empt the currently running task, only if it's running
                sparam.sched_priority=0;
                if (currently_running_task->state == RUNNING) {
                    currently_running_task->state = READY;
                }
                sched_setscheduler(currently_running_task->task, SCHED_NORMAL, &sparam);
            }

            // context switch to the chosen task
            currently_running_task = next_task;
            next_task->state = RUNNING;
            wake_up_process(next_task->task);
            sparam.sched_priority=90;
            sched_setscheduler(next_task->task, SCHED_FIFO, &sparam);

        }
        
        // use set_current_state and schedule to put the dispatching thread to sleep
        sparam.sched_priority=0;
        sched_setscheduler(dispatch_thread.task, SCHED_NORMAL, &sparam);
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();        
        printk(KERN_ALERT "Put dispatching thread to sleep\n");
    }
    return 0;
}

static void wake_up_timer_function(unsigned long arg){
    unsigned long flags;
    
    struct sched_param sparam;

    mp2_struct *waking_task = (mp2_struct *)arg;
    printk(KERN_ALERT "Timer bell rung! \t I belong to %u!\n", waking_task->pid);
    spin_lock_irqsave(&my_lock, flags);
    waking_task->state = READY;
    spin_unlock_irqrestore(&my_lock, flags);

    printk(KERN_ALERT "Waking up dispatch\n");
    wake_up_process(dispatch_thread.task);
    sparam.sched_priority=99;
    sched_setscheduler(dispatch_thread.task, SCHED_FIFO, &sparam);
}


static int admission_control(unsigned long new_task_period, unsigned long new_task_cputime) {
    unsigned long existing_ratio_sum;
    unsigned long task_ratio;
    unsigned long new_task_ratio;
    mp2_struct *i;

    new_task_ratio = 1000 * new_task_cputime / new_task_period;

    existing_ratio_sum = 0;
    mutex_lock(&mutex_list);
    list_for_each_entry(i, &head_task, task_node) {
        task_ratio = 1000 * i->cputime / i->period;
        existing_ratio_sum += task_ratio;
    }
    mutex_unlock(&mutex_list);

    return existing_ratio_sum + new_task_ratio <= 693;
}

static void REGISTER(unsigned int pid, unsigned long period, unsigned long cputime) {
    mp2_struct *new_task;
    int passes_admission_control;

    printk(KERN_ALERT "Received REGISTER message: %u\n", pid);

    // admission control
    passes_admission_control = admission_control(period, cputime);

    if (!passes_admission_control) {
        return;
    }

    // allocate mp2_struct
    new_task = kmem_cache_alloc(cache,0);
    if (new_task == NULL) {
        printk(KERN_ALERT "GOT A NULLPTR FROM kmem_cache_alloc\n");
    }
    // initialize mp2_struct
    new_task->pid = pid;
    new_task->cputime = cputime;
    new_task->period = period; 

    new_task->state = SLEEPING;
    

    new_task->deadline = jiffies + msecs_to_jiffies(new_task->period);

    /* setup your timer to call wakeupfun */
    setup_timer(&(new_task->task_timer), wake_up_timer_function, (unsigned long) new_task);

    new_task->task = find_task_by_pid(new_task->pid);

    // printk(KERN_ALERT "successfully set task structure to sleeping\n");
    list_add(&(new_task->task_node), &head_task);

}
static void DEREGISTRATION(unsigned int pid){
    mp2_struct *cursor;
    mp2_struct *next;
    
    printk(KERN_ALERT "DEREGISTRATION for pid: %u\n", pid);

    mutex_lock(&mutex_list);
    list_for_each_entry_safe(cursor, next, &head_task, task_node) {
        if (cursor->pid == pid){
            del_timer(&(cursor->task_timer));
            list_del(&(cursor->task_node));
            kmem_cache_free(cache,cursor);
            printk(KERN_ALERT "FOUND PID and deleted!!\n");
            // break; // stop after removing first pid found
        }    
    }   
    mutex_unlock(&mutex_list);
}

static void YIELD(unsigned int pid){
    mp2_struct *i;
    mp2_struct *next;
    unsigned long now;
    struct sched_param sparam;

    printk(KERN_ALERT "YIELDING for pid: %u\n", pid);

    mp2_struct *yielding_task = NULL;
    mutex_lock(&mutex_list);
    list_for_each_entry(i, &head_task, task_node) {
        if (i->pid == pid) {
            printk(KERN_ALERT "Found my PID!!\n");
            
            yielding_task = i;
            // printk(KERN_ALERT "FOUND PID!!\n");
            // spin_lock_irqsave(&mp2_spin_lock, 0);
            // spin_unlock_irqrestore(&mp2_spin_lock, 0);
            // set_task_state(i->task, TASK_UNINTERRUPTIBLE);
            // break;
            
        }    
    }
    mutex_unlock(&mutex_list);
    if (yielding_task != NULL && yielding_task->task != NULL) {
        if (jiffies < yielding_task->deadline) {
            printk(KERN_ALERT "Before the deadline, should sleep\n");
            // set the state of the yielding task to sleeping
            yielding_task->state = SLEEPING;
            // calculate the next release time
            yielding_task->deadline += msecs_to_jiffies(yielding_task->period);
            /* setup timer interval to period jiffies */
            mod_timer(&(yielding_task->task_timer), yielding_task->deadline);
            // put the task to sleep as task uniterruptible
            set_task_state(yielding_task->task, TASK_UNINTERRUPTIBLE);
            sparam.sched_priority = 0;
            sched_setscheduler(dispatch_thread.task, SCHED_NORMAL, &sparam);
            schedule();
            printk(KERN_ALERT "Process %u is sleeping now\n", yielding_task->pid);
        } else { // missed our deadline
            printk(KERN_ALERT "Missed the deadline\n");
            yielding_task->deadline += msecs_to_jiffies(yielding_task->period);
        }

    }


}

static ssize_t mp2_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
    int copied;
    char * buf;
    char * line;
    int line_length;
    char * buf_curr_pos;
    mp2_struct *i;
    
    // printk(KERN_ALERT "mp2_read called with %zu bytes\n", count);

    // must return 0 to signal that we're done writing to cat
    if (finished_writing == 1) {
        finished_writing = 0;
        // printk(KERN_ALERT "mp2_read finished\n");
        return 0;
    } 

    copied = 0;

    // allocate a buffer big enough to hold the contents
    buf = (char *) kmalloc(READ_BUFFER_SIZE, GFP_KERNEL);
    memset(buf, 0, READ_BUFFER_SIZE);
    buf_curr_pos = buf;
    // mutex_lock(&mutex_list);
    list_for_each_entry(i, &head_task, task_node) {

        // printk(KERN_ALERT "on entry pid: %u \n", i->pid);
        // allocate line long enoguh to hold the string
        line = kmalloc(LINE_LENGTH, GFP_KERNEL);
        memset(line, 0, LINE_LENGTH);

        sprintf(line, "PID: %u, period: %lu\n", i->pid, i->period);
        line_length = strlen(line);
        
        snprintf(buf_curr_pos, line_length + 1, "%s", line); // + 1 to account for the null char
        buf_curr_pos += line_length; // advance the buffer 

        kfree(line);
    }
    // mutex_unlock(&mutex_list);

    copy_to_user(buffer, buf, READ_BUFFER_SIZE);
    finished_writing = 1; // return 0 on the next run
    kfree(buf);
    return READ_BUFFER_SIZE;
}

static ssize_t mp2_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){ 
    int copied;
    char *buf;

    unsigned int pid;
    unsigned long period;
    unsigned long cputime;
    
    // printk(KERN_ALERT "mp2_write called with %zu bytes\n", count);
    // manually null terminate
    buf = (char *) kmalloc(count+1,GFP_KERNEL); 
    copied = copy_from_user(buf, buffer, count);
    buf[count]=0;

    if (count > 0) {
        char cmd = buf[0];
        switch (cmd){
            case 'R':
                sscanf(buf + 3, "%u, %lu, %lu\n", &pid, &period, &cputime);
                REGISTER(pid,period,cputime);
                break;
            case 'Y':
                sscanf(buf + 3, "%u\n", &pid);
                YIELD(pid);
                break;
            case 'D':
                sscanf(buf + 3, "%u\n", &pid);
                DEREGISTRATION(pid);
                break;
        }

        kfree(buf);
    }
    return count;
}

static const struct file_operations mp2_file = {
    .owner = THIS_MODULE, 
    .read = mp2_read,
    .write = mp2_write,
};

// mp2_init - Called when module is loaded
int __init mp2_init(void)
{

    cache = kmem_cache_create("cache_name", sizeof(mp2_struct), 0, 0, NULL);
    /** does not compile, context switch undefined, TODO fix */

    #ifdef DEBUG
        printk(KERN_ALERT "MP2 MODULE LOADING\n");
    #endif

    // set up procfs
    proc_dir = proc_mkdir(DIRECTORY, NULL);
    proc_entry = proc_create(FILENAME, 0666, proc_dir, & mp2_file); 
    
    dispatch_thread.task = kthread_run(&dispatch_thread_function, NULL, "dispatch_thread");

    return 0;   
}

// mp2_exit - Called when module is unloaded
void __exit mp2_exit(void)
{
    #ifdef DEBUG
        printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
    #endif
    
    // remove the procfs entry first so that users can't access it while we're deleting the list
    proc_remove(proc_entry);
    proc_remove(proc_dir);
    
    #ifdef DEBUG
        printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
    #endif
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);
