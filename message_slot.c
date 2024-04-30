
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. */
#include <linux/slab.h>

MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations
#include "message_slot.h"

struct chardev_info {
    spinlock_t lock;

};

// used to prevent concurrent access into the same device
static int dev_open_flag = 0;

static struct chardev_info device_info;

// The message the device will give when asked

struct channel_id{

    struct channel_id *head;
    struct channel_id *next;
    int ch_id;
    char message[BUF_LEN];
    unsigned int minor;
};

struct msg_slot_info{
    struct inode *inode;
    struct msg_slot_info *next;
    unsigned int minor;
    struct channel_id *channels_list_head;
};

static struct msg_slot_info *msg_slot_root = NULL;

struct msg_slot_info *find_minor_in_list(unsigned int minor);
struct msg_slot_info *add_msg_slot( struct inode* inode);
void delete_msg_slot(struct inode* inode);
struct channel_id * add_channel(struct msg_slot_info * msg_slot, unsigned int new_channel_id);
struct channel_id * create_channel_node(unsigned int minor);
void  free_channels_list(struct msg_slot_info *msg_slot);
//================== DEVICE FUNCTIONS ===========================

static int device_open( struct inode* inode, struct file*  file )
{
    struct msg_slot_info * msg_slot;
    unsigned long flags; // for spinlock
    printk("Invoking device_open(%p)\n", file);

    file->private_data = (struct channel_id *) create_channel_node(iminor(inode));
    if ( find_minor_in_list(iminor(inode)) == NULL){
        //if the msg slot is new
        msg_slot = add_msg_slot(inode);
        msg_slot->inode = inode ;
    }
    ((struct channel_id *)file->private_data)->ch_id = 0;
    // We don't want to talk to two processes at the same time
    spin_lock_irqsave(&device_info.lock, flags);
    if( 1 == dev_open_flag ) {
        spin_unlock_irqrestore(&device_info.lock, flags);
        return -EBUSY;
    }


    ++dev_open_flag;
    spin_unlock_irqrestore(&device_info.lock, flags);

    return SUCCESS;
}

//---------------------------------------------------------------
static int device_release( struct inode* inode,
                           struct file*  file)
{
    unsigned long flags; // for spinlock
    printk("Invoking device_release(%p,%p)\n", inode, file);

    // ready for our next caller
    spin_lock_irqsave(&device_info.lock, flags);
    --dev_open_flag;
    spin_unlock_irqrestore(&device_info.lock, flags);
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                                size_t       length,
                                loff_t*      offset )
{

    size_t len_msg;
    printk(" message 1: %s\n", (((struct channel_id *)file->private_data)->message));
    printk(" file->private data: %s\n", (((struct channel_id *)file->private_data)->message));
    // if no channel has been set on the file descriptor
    if(file->private_data == NULL  ){
        return -EINVAL;
    }
    if (((struct channel_id *)file->private_data)->ch_id ==0 ){
        return -EINVAL;
    }

    len_msg = strlen(((struct channel_id *)file->private_data)->message);
    printk(" len_msg: %ld\n",  len_msg);


    printk( "Invocing device_read(%p,%ld) - " "operation not supported yet\n" "(last written - %s)\n", file, length, (((struct channel_id *)file->private_data)->message) );

    //If no message exists on the channel
    if(len_msg == 0){


        return -EWOULDBLOCK;;
        }
            printk(" message 2: %s\n", (((struct channel_id *)file->private_data)->message));
            // If the buffer length is too small to hold the last message written on the channel
            if (len_msg > length ){

                printk(" problem a length: %ld\n",  length);
                printk(" problem a len_msg: %ld\n",  len_msg);
                return -ENOSPC;
                }
                //reading into buffer
            if (0 != copy_to_user(buffer, (((struct channel_id *)file->private_data)->message), len_msg)) {
                    printk(" error in copy\n");

            return -EFAULT;
                }
            printk(" buffer 2: %s\n", buffer);
            return len_msg;
            }

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                                size_t             length,
                                 loff_t*            offset)
                                    {
                                ssize_t i;

    char temp_buffer[BUF_LEN];
    printk("Invoking device_write(%p,%ld)\n", file, length);
    // if no channel has been set on the file descriptor
    if(file->private_data == NULL){
    return -EINVAL;
    }
    if (((struct channel_id *)file->private_data)->ch_id ==0 ){
        return -EINVAL;
    }



    for( i = 0; i  < BUF_LEN; ++i ){
        temp_buffer[i] =  (((struct channel_id *)file->private_data)->message)[i];
        (((struct channel_id *)file->private_data)->message)[i] ='\0';

        }

    for( i = 0; i < length && i < BUF_LEN; ++i ) {
        if( get_user((((struct channel_id *)file->private_data)->message)[i], &buffer[i]) != 0){
        // if get user failed{
    for( i = 0; i  < BUF_LEN; ++i ){
        (((struct channel_id *)file->private_data)->message)[i] = temp_buffer[i];
        return -EINVAL;
        }

              }

              }

        printk("Received message: %s\n", (((struct channel_id *)file->private_data)->message));
        // if error in passed message length
        if (i==0 || length > BUF_LEN) {
            return -EMSGSIZE;
        }
        // return the number of input characters used
        printk("number of bytes i: %ld\n",i);
        return i;
        }

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  chanel_id )
{
    unsigned int the_minor;
    struct msg_slot_info *msg_slot;
    struct channel_id *updated_channel;
    if(( MSG_SLOT_CHANNEL != ioctl_command_id )  ||  (chanel_id == 0)){

        return -EINVAL;
    }
    //find minor and search for channel.
    //use the minor and update channel.
    the_minor =  ((struct channel_id *)file->private_data)->minor;
    printk("this is the minor: %u\n",the_minor);
    msg_slot =  find_minor_in_list(the_minor);
    // find channel in list of channels at msg_slot
    updated_channel = add_channel(msg_slot, chanel_id);
    file->private_data  = (struct channel_id *) updated_channel;
    updated_channel->minor = the_minor;

    return SUCCESS;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
        .owner	  = THIS_MODULE,
        .read           = device_read,
        .write          = device_write,
        .open           = device_open,
        .unlocked_ioctl = device_ioctl,
        .release        = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
    int rc = -1;
    // init dev struct
    memset( &device_info, 0, sizeof(struct chardev_info) );
    spin_lock_init( &device_info.lock );

    // Register driver capabilities. Obtain major num
    rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

    // Negative values signify an error
    if( rc < 0 ) {
        printk( KERN_ALERT "%s registraion failed for  %d\n",
                DEVICE_FILE_NAME, MAJOR_NUM );
        return rc;
    }

    printk( "Registeration is successful. ");
    printk( "If you want to talk to the device driver,\n" );
    printk( "you have to create a device file:\n" );
    printk( "mknod /dev/%s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM );
    printk( "You can echo/cat to/from the device file.\n" );
    printk( "Dont forget to rm the device file and "
            "rmmod when you're done\n" );

    return 0;
}

//additional functions

struct msg_slot_info *find_minor_in_list(unsigned int minor){
    struct msg_slot_info *curr_node;
    if(msg_slot_root == NULL){
        return NULL;
    }
    curr_node = msg_slot_root;
    while (curr_node != NULL){
        if (curr_node->minor == minor){
            return curr_node;
        }
        curr_node = curr_node->next;
    }
    return NULL;
}

struct msg_slot_info *add_msg_slot( struct inode* inode){
    struct msg_slot_info *new_node;
    struct msg_slot_info* curr_node;
    new_node = kmalloc(sizeof(struct msg_slot_info), GFP_KERNEL);
    if (!new_node) {
        return NULL;
    }
    // new_node->inode = inode;
    new_node->minor = iminor(inode);
    new_node->channels_list_head = NULL;
    //new_node->channel_id;


    if ( msg_slot_root == NULL){
        msg_slot_root = new_node;
    }
    else {
        //not empty list
        curr_node = msg_slot_root;
        while (curr_node->next != NULL){
            curr_node = curr_node->next;
        }
        curr_node->next = new_node;
    }
    new_node->next = NULL;
    return new_node;
}

void delete_msg_slot(struct inode* inode){  
    //struct msg_slot_info *node_to_delete;
    struct msg_slot_info *node_to_delete;
    struct msg_slot_info* curr_node;
    curr_node = msg_slot_root;
    if (curr_node == NULL){
        printk(KERN_ALERT "There are no message slots \n");   
        return;
    }
    if(curr_node->inode == inode){
        msg_slot_root = curr_node->next; //
        free_channels_list(curr_node);
        kfree(curr_node);
        return;
    }
    while(curr_node->next != NULL && curr_node->next->inode != inode){
        curr_node = curr_node->next;
    }
    if (curr_node->next == NULL){
        printk(KERN_ALERT "The message slot does not exist \n");
        return;
    }

    node_to_delete = curr_node->next;
    curr_node->next = curr_node->next->next;
    free_channels_list(node_to_delete);
    kfree(node_to_delete);

}

//given msg_slot and id chanel if not exist: add chanel to channels of msg_slot.
//if exsist return it
struct channel_id * add_channel(struct msg_slot_info * msg_slot, unsigned int new_channel_id){

    struct channel_id * new_node = kmalloc(sizeof(struct channel_id), GFP_KERNEL);
    if (!new_node) {
        return NULL; //kmalloc
    }
    new_node->ch_id = new_channel_id;
    if (msg_slot->channels_list_head == NULL){
        //first channel in this msg slot
        msg_slot->channels_list_head = new_node;
        new_node->head = new_node;
        new_node->next = NULL;
    }
    else {
        //not empty list
        struct channel_id *curr_node = msg_slot->channels_list_head;
        if(curr_node->ch_id == new_channel_id ){
            return curr_node;
        }
        while (curr_node->next != NULL){
            curr_node = curr_node->next;
            if(curr_node->ch_id == new_channel_id ){
                //if channel exsists - return it
                return curr_node;
            }
        }
        curr_node->next = new_node;
    }
    new_node->next = NULL;
    return new_node;

}

//create new node of channel with given minor
struct channel_id * create_channel_node(unsigned int minor){
    struct channel_id *new_channel_node;
    new_channel_node = kmalloc(sizeof(struct channel_id), GFP_KERNEL);
    if (!new_channel_node) {
        return NULL;
    }
    new_channel_node->minor = minor;
    new_channel_node->head = NULL;
    new_channel_node->next = NULL;
    return new_channel_node;
}
void  free_channels_list(struct msg_slot_info *msg_slot){
    struct channel_id *curr_node = msg_slot->channels_list_head;
    struct channel_id  *node_to_delete;
    while(curr_node != NULL){
        node_to_delete = curr_node;
        curr_node = curr_node->next;
        kfree(node_to_delete);
    }
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
    // Unregister the device
    // Should always succeed
    struct msg_slot_info *curr_node = msg_slot_root;
    struct msg_slot_info *node_to_delete;

    while (curr_node != NULL) {
        node_to_delete = curr_node;
        curr_node = curr_node->next;
        free_channels_list(node_to_delete);
        kfree(node_to_delete);
    }
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
