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
static spinlock_t mp2_spin_lock;
#define SLEEPING 0
#define READY 1
#define RUNNING 2
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


// helpers for timer code
static struct timer_list myTimer;
/*void timerFun (unsigned long arg) {
    myTimer.expires = jiffies + 5*HZ;
    add_timer (&myTimer); // setup the timer again /
    schedule_work(&wq); // trigger the bottom half
}
*/
typedef struct mp2_task_struct {
    struct task_struct *task;
    struct list_head task_node;
    struct timer_list task_timer;
    unsigned int state;
    uint64_t next_period;
    unsigned int pid;
    unsigned long cputime;
    unsigned long period;
    unsigned long slice;
} mp2_struct;

static mp2_struct dispatch_thread;
static mp2_struct*  live_task;

static LIST_HEAD(head_task);
static int finished_writing;

static void update_tasks(void) {
    mp2_struct *i;
    struct sched_param sparam; 

    list_for_each_entry(i, &head_task, task_node) {
        if (live_task != NULL) {
            sparam.sched_priority=0;    
            sched_setscheduler(i->task, SCHED_NORMAL, &sparam);
        }   
        if (i->task->state == READY && i->cputime < live_task->cputime){
             if (live_task != NULL && i != NULL && live_task->state == RUNNING){
                // spin_lock_irqsave(&mp2_spin_lock, 0);
                live_task->state = READY;
                spin_unlock_irqrestore(&mp2_spin_lock, 0);    
             }
        }
        if (live_task == NULL || i->cputime < live_task->cputime) {
            // spin_lock_irqsave(&mp2_spin_lock, 0);
            i->state = RUNNING;
            spin_unlock_irqrestore(&mp2_spin_lock, 0);
            
            wake_up_process(i->task);
            sparam.sched_priority=99;
            sched_setscheduler(i->task, SCHED_FIFO, &sparam);
            live_task = i;
        }
    }
}
static int thread_fun(void *arg){
    for(;;){
        update_tasks();
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();        
    }
    return 0;
}

static void wake_up_timer_handler(mp2_struct *task){
    // spin_lock_irqsave(&mp2_spin_lock, 0);
    if (live_task != task){
        task->state = READY;
        // wake_up_process(dispatch_thread);
    }
    spin_unlock_irqrestore(&mp2_spin_lock, 0);
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

        sprintf(line, "PID: %lu, cputime: %lu\n", i->pid, i->cputime);
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
static void REGISTER(unsigned int pid, unsigned long period, unsigned long computation){
    mp2_struct *mp2_task;
    mp2_task = kmem_cache_alloc(cache,0);
    if (mp2_task == NULL) {
        printk(KERN_ALERT "GOT A NULLPTR FROM kmem_cache_alloc\n");
    }
    mp2_task->pid = pid;
    mp2_task->cputime = computation;
    mp2_task->period = period; 
    // TODO to add computation
    mp2_task->task = find_task_by_pid(pid);
    mp2_task->task->state = SLEEPING;
    printk(KERN_ALERT "successfully set task structure to sleeping\n");
    list_add(&(mp2_task->task_node), &head_task);

    // mp2_struct *cursor;
    // mp2_struct *next;
    // list_for_each_entry_safe(cursor, next, &head_task, task_node) {
    //     printk(KERN_ALERT "have pid: %u, cputime: %lu, period: %lu\n", cursor->pid, cursor->cputime, cursor->period);
    // }
}
static void DEREGISTRATION(unsigned int pid){
    mp2_struct *cursor;
    mp2_struct *next;

    list_for_each_entry_safe(cursor, next, &head_task, task_node) {
        if (cursor->pid == pid){
            list_del(&(cursor->task_node));
            kmem_cache_free(cache,cursor);
            // printk(KERN_ALERT "FOUND PID and deleted!!\n");
            break; // stop after removing first pid found
        }    
    }   
    
}

static void YIELD(unsigned int pid){
    // printk(KERN_ALERT "YIELDING for pid: %u\n", pid);
    mp2_struct *cursor;
    mp2_struct *next;

    list_for_each_entry_safe(cursor, next, &head_task, task_node) {
        if (cursor->pid == pid){
            // printk(KERN_ALERT "FOUND PID!!\n");
            // spin_lock_irqsave(&mp2_spin_lock, 0);
            // cursor->state = SLEEPING;
            // spin_unlock_irqrestore(&mp2_spin_lock, 0);
            // set_task_state(cursor->task, TASK_UNINTERRUPTIBLE);
            // break;
        }    
    }
}

static ssize_t mp2_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){ 
    int copied;
    char *buf;
    list_node *new_node;
    unsigned int pid;
    unsigned long period;
    unsigned long computation;
    
    printk(KERN_ALERT "mp2_write called with %u bytes\n", count);
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
                sscanf(buf + 3, "%u, %lu, %lu\n", &pid, &period, &computation);

                // printk(KERN_ALERT "WE GOD DIS : pid %u perid %lu comp %lu\n", pid, period, computation);
                // printk(KERN_ALERT "CALLING REGISTER()\n");
                REGISTER(pid,period,computation);
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
    unsigned long currentTime;
    unsigned long expiryTime;
    cache = kmem_cache_create("cache_name", sizeof(mp2_struct), 0, 0, NULL);
    /** does not compile, context switch undefined, TODO fix */

    #ifdef DEBUG
        printk(KERN_ALERT "MP2 MODULE LOADING\n");
    #endif

    // set up procfs
    proc_dir = proc_mkdir(DIRECTORY, NULL);
    proc_entry = proc_create(FILENAME, 0666, proc_dir, & mp2_file); 
    
    dispatch_thread.task = kthread_run(&thread_fun, NULL, "dispatch_thread");
    // set up timer interrupt
    // currentTime = jiffies; // pre-defined kernel variable jiffies gives current value of ticks
    // expiryTime = currentTime + 5*HZ; 
    // init_timer (&myTimer);
    // //myTimer.function = timerFun;
    // myTimer.expires = expiryTime;
    // myTimer.data = 0;
    //add_timer (&myTimer);

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
    list_for_each_entry_safe(cursor, next, &head, list) {
        list_del(&(cursor->list));
        kfree(cursor);
    }

    #ifdef DEBUG
        printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
    #endif
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);
