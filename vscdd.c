/*
 * Шаблон для разработки драйвера символьного устройства
 * vscdd.c -- very simple chrdev driver
 */

#include <linux/module.h>
#include <linux/moduleparam.h>  /* command-line args */
#include <linux/init.h>
#include <linux/kernel.h>       /* printk() */
#include <linux/slab.h>        /* kfree(), kmalloc() */
#include <linux/fs.h>           /* file_operations */
#include <linux/types.h>        /* dev_t */
#include <linux/cdev.h>
#include <asm/uaccess.h>        /* copy_*_user */

#include <linux/version.h> 
#include <linux/device.h>

#ifndef MODULE_NAME
#define MODULE_NAME "vscdd"
#endif

/*
 *  Параметры модуля, которые можно передать при загрузке
 *  insmod ./vscdd.ko major=345 minor=5 count=3
 */
int major = 0; // старший номер устройства, 0 - динамическое выделение
int minor = 0; // младший номер первого устройства
int count = 1; // количество устройств, для каждого нужен отдельный файл
module_param(major, int, S_IRUGO);
module_param(minor, int, S_IRUGO);
module_param(count, int, S_IRUGO); 

/*
 * Счетчик использования устройства
 */
static int device_open = 0;

static struct class *devclass;

/* 
 * Структура драйвера символьного устройства
 */ 
struct cdev *cdev;

/* 
 * Память устройства
 */
static char *vscdd_buffer;

static int cur_size = 0;

/*
 * Функция открытия устройства
 */
int vscdd_open(struct inode *inode, struct file *filp)
{
    if (device_open) {
        printk("=== vscdd: device is already opened ===\n");
        return -EBUSY;
    }
    device_open++;
    printk("=== vscdd: opening device ===\n");
    try_module_get(THIS_MODULE);
    return 0;
}

/*
 * Функция освобождения устройства
 */
int vscdd_release(struct inode *inode, struct file *filp)
{
    if (!device_open) 
        return -EFAULT;
    device_open--;
    module_put(THIS_MODULE);
    return 0;
}

/*
 * Функция чтения из устройства
 */
ssize_t vscdd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
    // код возврата
    ssize_t retval = 0;

    // количество байтов, которое осталось прочитать
    int remain = 100 - (int) (*f_pos); 
    if (remain == 0)
        return 0;
    if (count > remain)
        count = remain;

    if (copy_to_user(buf /* куда */, vscdd_buffer + *f_pos /* откуда */, count /* сколько */)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;

    out:
        return retval;
}

/*
 * Функция записи в устройство
 */
ssize_t vscdd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    // код возврата
    ssize_t retval = 0;

    // количество байтов, которое осталось записать
    int remain = 100 - (int) (*f_pos); 

    if (count > remain) {
        // нельзя писать за пределы памяти устройства
        return -EIO;
    }

    if (copy_from_user(vscdd_buffer + *f_pos /* куда */, buf /* откуда */, count /* сколько */)) {
        retval = -EFAULT;
        goto out;
    }
    // увеличить значение указателя
    *f_pos += count;
    cur_size += count;
    retval = count;

    out:
        return retval;
}

loff_t vscdd_llseek(struct file *filp, loff_t offset, int origin)
{
    loff_t new_pos;

    printk("=== vscdd: f_pos = %lld ===\n", filp->f_pos); 
    printk("=== vscdd: offset = %lld ===\n", offset);
 
    switch(origin) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = filp->f_pos + offset;
            break;
        case SEEK_END:
            new_pos = cur_size + offset;
            break;
        default:
            return -EINVAL;
    }
    printk("=== vscdd: new_pos = %lld===\n", new_pos);

    if (new_pos<0) 
        return -EINVAL;
    filp->f_pos = new_pos;
    return new_pos;
}

/*
 * Функция драйвера для работы с устройством
 */
struct file_operations vscdd_fops = {
    .owner =    THIS_MODULE,
    .llseek = vscdd_llseek,
    .read =     vscdd_read,
    .write =    vscdd_write,
    .open =     vscdd_open,
    .release =  vscdd_release,
};

/* 
 * Функция выгрузки модуля и освобождения драйвера
 */
static void vscdd_exit(void) 
{

    dev_t dev;
    dev_t devno = MKDEV(major, minor);
    int i;
    for( i = 0; i < count; i++ ) {
        dev = MKDEV( major, minor + i );
        device_destroy( devclass, dev );
    }
    class_destroy( devclass );

    if (cdev) {
        cdev_del(cdev);
    }

    if (vscdd_buffer) 
        kfree(vscdd_buffer);

    // Освобождаем старшие и младшие номера устройств.
    unregister_chrdev_region(devno, count);

    printk("=== vscdd: exit ===\n");
}

/* 
 * Функция загрузки модуля и инициализации драйвера
 */
static int __init vscdd_init(void)
{
    int result;
    int i;
    dev_t dev = 0;
    result = 0;

    if (major) { 
        // если пользователь указал старший номер через параметр
        dev = MKDEV(major, minor);
        result = register_chrdev_region(dev, count, MODULE_NAME);
    } else {
        // если major == 0, то получить старший номер динамически
        result = alloc_chrdev_region(&dev, minor, count, MODULE_NAME);
           // зафиксировать динамически полученный старший номер
        major = MAJOR(dev);
    }

    if (result < 0) { 
        printk("=== vscdd: can't get a majori ===\n");
        return result;
    }

    cdev = cdev_alloc();
     if (!cdev) {
        result = -ENOMEM;
        goto fail;
    }
    cdev_init(cdev, &vscdd_fops);
    cdev->owner   = THIS_MODULE;
    if (cdev_add(cdev, dev, count)) { 
        printk("=== vscdd: cdev_add error ===\n");
    }
    printk( "=== vscdd: %d:%d ===\n", major, minor);

    devclass = class_create( THIS_MODULE, "vscdd_class" ); 
    for( i = 0; i < count; i++ ) {
        #define DEVNAME "vscdd"
        dev = MKDEV( major, minor + i );
        #if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
        device_create( devclass, NULL, dev, "%s_%d", DEVNAME, i );
        #else
        device_create( devclass, NULL, dev, NULL, "%s_%d", DEVNAME, i );
        #endif
    }
    printk( "=== vscdd: module installed %d:[%d-%d] ===\n", MAJOR(dev), minor, MINOR(dev) ); 

    vscdd_buffer = kzalloc(100 * sizeof (*vscdd_buffer), GFP_KERNEL);
    if (!vscdd_buffer) {
        result = -ENOMEM;
        goto fail;
    }

    return 0;

    fail:
        vscdd_exit();
        printk("=== vscdd: register error ===\n");
        return result; 
}


/*
 * Функции загрузки и выгрузки модуля
 */
module_init(vscdd_init);
module_exit(vscdd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("МИФИ");
MODULE_DESCRIPTION("Шаблон для разработки драйвера символьного устройства");
MODULE_VERSION("20160524");
