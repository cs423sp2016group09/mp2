#define LINUX
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h> 
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/cache.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_09");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG 1
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;
static struct kmem_cache *cache;

#define SLEEPING 0
#define READY 1
#define RUNNING 2
#define FILENAME "status"
#define DIRECTORY "mp1"
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
struct mp2_task_struct{
    struct task_struct *task;
    struct list_head task_node;
    struct timer_list task_timer;
    unsigned int task_state;
    uint64_t next_period;
    unsigned int pid;
    unsigned long relative_period;
    unsigned long slice;
} mp2_struct;

static LIST_HEAD(head_task);
static int finished_writing;

static ssize_t mp1_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
    int copied;
    char * buf;
    char * line;
    int line_length;
    char * buf_curr_pos;
    struct mp2_task_struct *i;

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

        sprintf(line, "PID: %d, relative_period: %lu\n", i->pid, i->relative_period);
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
static void REGISTER(unsigned int pid, unsigned long period, unsigned int computation){
    struct mp2_task_struct *mp2_task;
    mp2_task = kmem_cache_alloc(cache,0);
    mp2_task->pid = pid;
    mp2_task->relative_period = period; 
    //TO DO to add computation
    mp2_task->task->state = SLEEPING;
    list_add(&(mp2_task->task_node), &head_task);

}
static ssize_t mp1_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){ 
    int copied;
    char *buf;
    list_node *new_node;
    unsigned int pid;
    unsigned long period;
    unsigned int computation;
    switch (buffer[0]){
        case 'R':
            sscanf(&buffer[1], "%u, %lu, %u", &pid, &period, &computation);
	    REGISTER(pid,period,computation);
	    break;
        case 'Y':
	   // YIELD();
	    break;
 	case 'D':
	   // DEREGISTRATION();
	    break;
    }

    // manually null terminate
    buf = (char *) kmalloc(count+1,GFP_KERNEL); 
    copied = copy_from_user(buf, buffer, count);
    buf[count]=0;

    // get pid from char *
    kstrtoint(buf, 10, &pid);

    // create neew node
    new_node = kmalloc(sizeof(list_node), GFP_KERNEL);
    memset(new_node, 0, sizeof(list_node));
    new_node->pid = pid;

    // put the node in the linked list
    mutex_lock_interruptible(&mutex_list);
    list_add(&(new_node->list), &head);
    mutex_unlock(&mutex_list);

    kfree(buf);
    return count;
}

static const struct file_operations mp1_file = {
    .owner = THIS_MODULE, 
    .read = mp1_read,
    .write = mp1_write,
};

// mp1_init - Called when module is loaded
int __init mp1_init(void)
{
    unsigned long currentTime;
    unsigned long expiryTime;
    cache = kmem_cache_create("cache_name", sizeof(mp2_struct), 0, 0, NULL);
    #ifdef DEBUG
        printk(KERN_ALERT "MP1 MODULE LOADING\n");
    #endif

    // set up procfs
    proc_dir = proc_mkdir(DIRECTORY, NULL);
    proc_entry = proc_create(FILENAME, 0666, proc_dir, & mp1_file); 

    // set up timer interrupt
    currentTime = jiffies; // pre-defined kernel variable jiffies gives current value of ticks
    expiryTime = currentTime + 5*HZ; 
    init_timer (&myTimer);
    //myTimer.function = timerFun;
    myTimer.expires = expiryTime;
    myTimer.data = 0;
    //add_timer (&myTimer);

    return 0;   
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void)
{
    list_node *cursor;
    list_node *next;
    #ifdef DEBUG
        printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
    #endif
	
	// remove the procfs entry first so that users can't access it while we're deleting the list
    proc_remove(proc_entry);
    proc_remove(proc_dir);
    
    // kill the timer
    del_timer (&myTimer);

	// remove list node from list, free the wrapping struct
    list_for_each_entry_safe(cursor, next, &head, list) {
        list_del(&(cursor->list));
        kfree(cursor);
    }

    #ifdef DEBUG
        printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
    #endif
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
