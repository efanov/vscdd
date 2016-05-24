/*
 * Шаблон для разработки драйвера символьного устройства
 * vscdd.c -- very simple chrdev driver
 */

#include <linux/types.h>        /* dev_t */
#include <linux/cdev.h>
#include <asm/uaccess.h>        /* copy_*_user */
#include <linux/version.h>
#include <linux/device.h>

#ifndef MODULE_NAME
#define MODULE_NAME "vscdd"
@@ -33,6 +35,7 @@ module_param(count, int, S_IRUGO);
static int device_open = 0;

static struct class *devclass;

@@ -48,14 +51,35 @@ static char *vscdd_buffer;
 */
int vscdd_open(struct inode *inode, struct file *filp)
{
        static int counter = 0;
        if (device_open) {
                printk("Device has already been opened.\n");
                return -EBUSY;
        }
        device_open++;
        printk("Device opened. Times = %d", ++counter);
        try_module_get(THIS_MODULE);
       return 0;
}

struct my_dev{
        struct scull_qset *data;
        int quantum;
        int qset;
        unsigned long size;
        unsigned int access_key;
    struct semaphore sem;
        struct cdev cdev;
};

/*
 * Функция освобождения устройства
 */
int vscdd_release(struct inode *inode, struct file *filp)
{
        if (!device_open) return -EFAULT;
        device_open--;
        module_put(THIS_MODULE);
       return 0;
}

@@ -69,6 +93,7 @@ ssize_t vscdd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_

       // количество байтов, которое осталось прочитать
       int remain = 100 - (int) (*f_pos);

       if (remain == 0)
               return 0;
       if (count > remain)
@@ -85,12 +110,13 @@ ssize_t vscdd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_
       return retval;
}

static int size = 0;

ssize_t vscdd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_po
       ssize_t retval = 0;

@@ -145,30 +133,68 @@ ssize_t vscdd_write(struct file *filp, const char __user *buf, size_t count, lof
       }

       *f_pos += count;
        size += count;
       retval = count;

  out:
       return retval;
}

loff_t dev_llseek(struct file *filp, loff_t off, int type){
        loff_t new_position;

        printk("===f_pos = %d===", filp->f_pos);
        printk("===off = %d===", off);
        switch(type){
                case SEEK_SET:
                        new_position = off;
                        break;
                case SEEK_CUR:
                        new_position = filp->f_pos + off;
                        break;
                case SEEK_END:
                        new_position = size + off;
                        break;
                default:
                        return -EINVAL;
           }
        printk("===new_position = %d===", new_position);
        if (new_position < 0) return -EINVAL;
        filp->f_pos = new_position;
        return new_position;
}

struct file_operations vscdd_fops = {
       .owner =    THIS_MODULE,
        //.llseek =   llseek,
        .llseek =   dev_llseek,
       .read =     vscdd_read,
       .write =    vscdd_write,
       .open =     vscdd_open,
       .release =  vscdd_release,
};




/*
 * Функция выгрузки модуля и освобождения драйвера
 */
static void __exit vscdd_exit(void)
{
        dev_t dev;
        int i;
       dev_t devno = MKDEV(major, minor);

        for (i = 0; i < count; i++){
                dev = MKDEV(major, minor + i);
                device_destroy(devclass, dev);
        }
        class_destroy(devclass);

       if (cdev) {
               cdev_del(cdev);
       }
@@ -149,7 +213,7 @@ static void __exit vscdd_exit(void)
 */
static int __init vscdd_init(void)
{
        int result;
        int result, i;
       dev_t dev = 0;
       result = 0;

@@ -181,6 +245,18 @@ static int __init vscdd_init(void)
       }
       pr_info( "=== vscdd: %d:%d ===\n", major, minor);

        devclass = class_create( THIS_MODULE, "vscdd_class");
        for (i = 0; i < count; i++){
#define DEVNAME "vscdd"
                dev = MKDEV(major, minor + i);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
                device_create(devclass, NULL, dev, "%s_%d", DEVNAME, i);
#else
                device_create(devclass, NULL, dev, NULL, "%s_%d", DEVNAME, i);
#endif
        }
        pr_info("==module installed %d:[%d-%d]==\n", MAJOR(dev), minor, MINOR(dev));

       vscdd_buffer = kzalloc(100 * sizeof (*vscdd_buffer), GFP_KERNEL);
       if (!vscdd_buffer) {
               result = -ENOMEM;
@@ -202,7 +278,7 @@ static int __init vscdd_init(void)
module_init(vscdd_init);
module_exit(vscdd_exit);
