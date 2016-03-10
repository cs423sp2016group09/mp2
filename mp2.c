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
#include <linux/mutex.h>
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
#define READ_BUFFER_SIZE 1600
#define LINE_LENGTH 80

// linked list helper code
typedef struct node
{
    struct list_head list;
    int pid;
    long unsigned cputime;
} list_node;

static LIST_HEAD(head);
DEFINE_MUTEX(mutex_list);


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

    mp2_struct *highest_priority_ready_task = NULL;
    mutex_lock_interruptible(&mutex_list);
    list_for_each_entry(i, &head_task, task_node) {
        if (i->state == READY) {
            if (highest_priority_ready_task == NULL) {
                highest_priority_ready_task = i;
            } else {
                if (i->period < highest_priority_ready_task->period) {
                    highest_priority_ready_task = i;
                }
            }
        }
    }
    mutex_unlock(&mutex_list);
    return highest_priority_ready_task;
}

static void handle_new_running_task(void) {
    mp2_struct *next_task;
    
    struct sched_param sparam;
    struct sched_param sparam2;

    next_task = find_shortest_period();

    if (next_task == NULL) { // no shortest period
        
        if (currently_running_task != NULL) {
            sparam2.sched_priority=0;
            if (currently_running_task->state == RUNNING) {
                currently_running_task->state = READY;
            }
            sched_setscheduler(currently_running_task->task, SCHED_NORMAL, &sparam2);
        }

    } else {

        next_task->state = RUNNING;
        wake_up_process(next_task->task);
        sparam.sched_priority=99;
        sched_setscheduler(next_task->task, SCHED_FIFO, &sparam);
        
        if (currently_running_task != NULL) {
            sparam2.sched_priority=0;
            if (currently_running_task->state == RUNNING) {
                currently_running_task->state = READY;
            }
            sched_setscheduler(currently_running_task->task, SCHED_NORMAL, &sparam2);
        }

    }


     

    /**
     *
        // spin_unlock_irqrestore(&mp2_spin_lock, 0);
     * 
     * DEADPOOL:
                    // if (currently_running_task != NULL && highest_priority_task_in_list != NULL && currently_running_task->state == RUNNING){
                    //     // spin_lock_irqsave(&mp2_spin_lock, 0);
                    //     currently_running_task->state = READY;
                    //     spin_unlock_irqrestore(&mp2_spin_lock, 0);    
                    // }

                    // if (currently_running_task == NULL || highest_priority_task_in_list->period < currently_running_task->period) {
                    //     // spin_lock_irqsave(&mp2_spin_lock, 0);
                    //     currently_running_task = highest_priority_task_in_list;
                    // }
     */
}

static int dispatch_thread_function(void *arg){
    while(1) { // woken up
        printk(KERN_ALERT "WOKEN UP\n");
        handle_new_running_task();
        // set_current_state(TASK_INTERRUPTIBLE);
        
        set_current_state(TASK_INTERRUPTIBLE);
        printk(KERN_ALERT "GOING BACK TO SLEEP\n");
        schedule();        
    }
    return 0;
}

static void wake_up_timer_function(unsigned long arg){
    unsigned long flags;
    spin_lock_irqsave(&my_lock, flags);

    currently_running_task->state = READY;
    wake_up_process(dispatch_thread.task);

    spin_unlock_irqrestore(&my_lock, flags);
}


static ssize_t mp2_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
    int copied;
    char * buf;
    char * line;
    int line_length;
    char * buf_curr_pos;
    mp2_struct *i;

    // must return 0 to signal that we're done writing to cat
    if (finished_writing == 1) {
        finished_writing = 0;
        return 0;
    } 

    copied = 0;

    // allocate a buffer big enough to hold the contents
    buf = (char *) kmalloc(READ_BUFFER_SIZE, GFP_KERNEL);
    memset(buf, 0, READ_BUFFER_SIZE);
    buf_curr_pos = buf;
    mutex_lock_interruptible(&mutex_list);
    list_for_each_entry(i, &head_task, task_node) {
        // allocate line long enoguh to hold the string
        line = kmalloc(LINE_LENGTH, GFP_KERNEL);
        memset(line, 0, LINE_LENGTH);

        sprintf(line, "PID: %u, cputime: %lu\n", i->pid, i->cputime);
        line_length = strlen(line);
        
        snprintf(buf_curr_pos, line_length + 1, "%s", line); // + 1 to account for the null char
        buf_curr_pos += line_length; // advance the buffer 

        kfree(line);
    }
    mutex_unlock(&mutex_list);

    copy_to_user(buffer, buf, READ_BUFFER_SIZE);
    finished_writing = 1; // return 0 on the next run
    kfree(buf);
    return READ_BUFFER_SIZE;
}

static int admission_control(unsigned long new_task_period, unsigned long new_task_cputime) {
    unsigned long existing_ratio_sum;
    unsigned long task_ratio;
    unsigned long new_task_ratio;
    mp2_struct *i;

    new_task_ratio = 1000 * new_task_cputime / new_task_period;

    existing_ratio_sum = 0;
    mutex_lock_interruptible(&mutex_list);
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
    
    new_task->task_timer.function = wake_up_timer_function;
    new_task->task_timer.data = 0;
    // time_t start, end, time_diff;

    // time(&start);
    // time(&end);

    // time_diff = difftime(end, start);
    
    new_task->deadline = jiffies_to_msecs(jiffies) + new_task->period;

    new_task->task = find_task_by_pid(pid);

    // printk(KERN_ALERT "successfully set task structure to sleeping\n");
    list_add(&(new_task->task_node), &head_task);

    // mp2_struct *cursor;
    // mp2_struct *next;
    // list_for_each_entry_safe(cursor, next, &head_task, task_node) {
    //     printk(KERN_ALERT "have pid: %u, cputime: %lu, period: %lu\n", cursor->pid, cursor->cputime, cursor->period);
    // }
}
static void DEREGISTRATION(unsigned int pid){
    mp2_struct *cursor;
    mp2_struct *next;

    mutex_lock_interruptible(&mutex_list);
    list_for_each_entry_safe(cursor, next, &head_task, task_node) {
        if (cursor->pid == pid){
            del_timer(&(cursor->task_timer));
            list_del(&(cursor->task_node));
            kmem_cache_free(cache,cursor);
            // printk(KERN_ALERT "FOUND PID and deleted!!\n");
            // break; // stop after removing first pid found
        }    
    }   
    mutex_unlock(&mutex_list);
}

static void YIELD(unsigned int pid){
    //printk(KERN_ALERT "YIELDING for pid: %u\n", pid);
    mp2_struct *i;
    mp2_struct *next;
    unsigned long now;
    struct sched_param sparam;



    mutex_lock_interruptible(&mutex_list);
    list_for_each_entry_safe(i, next, &head_task, task_node) {
        if (i->pid == pid){
            now = jiffies_to_msecs(jiffies);

            if (now < i->deadline) {
                i->state = SLEEPING;
                // calculate remaining time
                i->deadline += i->period;
                // set expiry time
                i->task_timer.expires = msecs_to_jiffies(i->deadline);
                add_timer (&i->task_timer);
		set_task_state(i->task, TASK_UNINTERRUPTIBLE);
		sparam.sched_priority = 0;
		sched_setscheduler(&dispatch_thread, SCHED_NORMAL, &sparam);
		schedule();
            } else { // missed our deadline
                i->deadline += i->period;
            }
            // printk(KERN_ALERT "FOUND PID!!\n");
            // spin_lock_irqsave(&mp2_spin_lock, 0);
            // spin_unlock_irqrestore(&mp2_spin_lock, 0);
            // set_task_state(i->task, TASK_UNINTERRUPTIBLE);
            // break;
        }    
    }
    mutex_unlock(&mutex_list);
}

static ssize_t mp2_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){ 
    int copied;
    char *buf;

    unsigned int pid;
    unsigned long period;
    unsigned long cputime;
    
    printk(KERN_ALERT "mp2_write called with %zu bytes\n", count);
    // manually null terminate
    buf = (char *) kmalloc(count+1,GFP_KERNEL); 
    copied = copy_from_user(buf, buffer, count);
    buf[count]=0;

    if (count > 0) {
        char cmd = buf[0];
        switch (cmd){
            case 'R':
                // printk(KERN_ALERT "GOT REGISTRATION MESSAGE\n");
                // printk(KERN_ALERT "string: %s\n", buf);
                sscanf(buf + 3, "%u, %lu, %lu\n", &pid, &period, &cputime);

                // printk(KERN_ALERT "WE GOD DIS : pid %u perid %lu comp %lu\n", pid, period, cputime);
                // printk(KERN_ALERT "CALLING REGISTER()\n");
                REGISTER(pid,period,cputime);
                break;
            case 'Y':
                // printk(KERN_ALERT "GOT YIELD MESSAGE\n");
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
    // set up timer interrupt
 
    // currentTime = jiffies; // pre-defined kernel variable jiffies gives current value of ticks
    // expiryTime = currentTime + 5*HZ; 
    // init_timer (&myTimer);
    // myTimer.function = wake_up_timer_function;
    // myTimer.expires = expiryTime;
    // myTimer.data = 0;
    // add_timer (&myTimer);

    return 0;   
}

// mp2_exit - Called when module is unloaded
void __exit mp2_exit(void)
{
    list_node *cursor;
    list_node *next;
    #ifdef DEBUG
        printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
    #endif
    
    // remove the procfs entry first so that users can't access it while we're deleting the list
    proc_remove(proc_entry);
    proc_remove(proc_dir);
    
    // kill the timer
    // del_timer (&myTimer);

    // remove list node from list, free the wrapping struct
    mutex_lock_interruptible(&mutex_list);
    list_for_each_entry_safe(cursor, next, &head, list) {
        list_del(&(cursor->list));
        kfree(cursor);
    }
    mutex_unlock(&mutex_list);

    #ifdef DEBUG
        printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
    #endif
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);
