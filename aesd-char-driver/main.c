/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Lior Weintraub"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev; /* device information */
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev; /* for other methods */
 
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    size_t offset_rtn=0;
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer,*f_pos,&offset_rtn);
    if(entry) {
        size_t bytes_to_copy = entry->size - offset_rtn;
        if(bytes_to_copy > count) {
            bytes_to_copy = count;
        }
        if(copy_to_user(buf,&entry->buffptr[offset_rtn],bytes_to_copy)) {
            retval = -EFAULT;
        } else {
            *f_pos += bytes_to_copy;
            retval = bytes_to_copy;
        }
    }
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct  aesd_dev *dev = filp->private_data;
    char *p_temp_message;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;     
    p_temp_message = kmalloc(count,GFP_KERNEL);
    if(!p_temp_message) {
        retval = -ENOMEM;
        goto out;
    }

    if(copy_from_user(p_temp_message,buf,count)) {
        retval = -EFAULT;
        kfree(p_temp_message);
        goto out;
    }
    if (p_temp_message[count-1] == '\n') {
        // If the last character is a newline, pop all previous temp buffers and write them to a single entry on the circular buffer
        struct temp_buffer_list *cursor, *temp;
        size_t total_size = count;
        struct  aesd_buffer_entry entry;
        list_for_each_entry_safe(cursor, temp, &dev->tbl, list) {
            total_size += cursor->size;
        }
        entry.buffptr = kmalloc(total_size,GFP_KERNEL);
        if(!entry.buffptr) {
            retval = -ENOMEM;
            goto out;
        }
        entry.size = 0;
        list_for_each_entry_safe(cursor, temp, &dev->tbl, list) {
            memcpy((void*)(entry.buffptr + entry.size), cursor->data, cursor->size);
            entry.size += cursor->size;
            list_del(&cursor->list);
            kfree(cursor->data);
            kfree(cursor);
        }
        memcpy((void*)(entry.buffptr + entry.size), p_temp_message, count);
        entry.size += count;
        // Free the temp buffer
        kfree(p_temp_message);
        // Check if the buffer is full
        if(dev->buffer.full) {
            // Deallocate the oldest entry because it is about to get overwritten
            kfree(dev->buffer.entry[dev->buffer.in_offs].buffptr);
        }
        aesd_circular_buffer_add_entry(&dev->buffer,&entry);
    } else {
        // If the last character is not a newline, add the buffer to the temp buffer list
        struct  temp_buffer_list *temp_buf;
        temp_buf = kmalloc(sizeof(struct temp_buffer_list),GFP_KERNEL);
        if(!temp_buf) {
            retval = -ENOMEM;
            goto out;
        }
        temp_buf->data = (void*)p_temp_message;
        temp_buf->size = count;
        list_add_tail(&temp_buf->list,&dev->tbl);
    }
    
    retval = count;
out:
    mutex_unlock(&dev->lock);
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.buffer);
    mutex_init(&aesd_device.lock);
    INIT_LIST_HEAD(&aesd_device.tbl);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct temp_buffer_list *cursor, *temp;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

     /* Go through the list and free the memory. */
    list_for_each_entry_safe(cursor, temp, &aesd_device.tbl, list) {
        printk(KERN_WARNING "Free entry with size %lu\n", cursor->size);
        list_del(&cursor->list);
        kfree(cursor);
    }

    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index) {
        kfree(entry->buffptr);
    }
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
