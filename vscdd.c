#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>
#include <asm/uaccess.h>
 
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
module_param(major, int, S_IRUGO);  //обязательный макрос (3ий параметр - права доступа)
module_param(minor, int, S_IRUGO);
module_param(count, int, S_IRUGO); 

/*
 * Счетчик использования устройства
 */
static int device_open = 0;
 
/*
 * Класс устройства
 */
static struct class *devclass;

/* 
 * Структура драйвера символьного устройства
 */ 
struct cdev *cdev;
 
/* 
 * Память устройства
 */
static char *vscdd_buffer;

/*
 * Размер файла
 */
static int filesize = 0;  

/*
 * Функция открытия устройства со счётчиком
 */
int vscdd_open(struct inode *inode,
               struct file *file)
{
    if (device_open != 0) {
		printk("Device has already been opened.\n");
		return -EBUSY;
	} 

    if (filp->f_flags & O_APPEND) {
        if (size < (100 * sizeof (*vscdd_buffer))){
            filp->f_pos = filesize + 1;
        }
        else
        {
            return -ENOMEM;
        }
    }
	++device_open; 
	try_module_get(THIS_MODULE); // inc count
 	return filp;
}
 
/*
 * Функция освобождения устройства
 */
int vscdd_release (struct inode *inode,
                   struct file *file)
{
	--device_open;
	module_put(THIS_MODULE); // dec count
 	return 0;
}
 
/*
 * Функция чтения из устройства
 */
ssize_t vscdd_read(struct file *file,
                          char __user *buf, 
                          size_t count, 
                          loff_t *f_pos)
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
ssize_t vscdd_write(struct file *file, 
                           const char __user *buf,
                           size_t count, 
                           loff_t *f_pos)
{
 	// код возврата
 	ssize_t retval = 0;
 	
 	// количество байт, которое осталось записать
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
 	retval = count;
    filesize += count; 
 
   out:
 	return retval;
}
 
 /*
  * Функция драйвера для работы с устройством
  */
struct file_operations vscdd_fops = 
{
    .owner    =  THIS_MODULE,
    .llseek   =  vscdd_llseek,
    .read     =  vscdd_read,
    .write    =  vscdd_write,
    .open     =  vscdd_open,
    .release  =  vscdd_release,
};

loff_t vscdd_llseek(struct file *filp, 
                           loff_t offset, 
                           int origin)
{
    if (origin == SEEK_CUR)
    {
      offset += filp->f_pos;
    }
    else if (origin == SEEK_END)
    {
      offset += filp->filesize;
    }

 	if (offset < 0) {
 		return -EINVAL;
 	}

    return offset;
}

/* 
 * Функция выгрузки модуля и освобождения драйвера
 */
static void __exit vscdd_exit(void) 
{    
 	dev_t devno = MKDEV(major, minor);
	
    int i = 0;
    while (i < count){
        device_destroy(devclass, MKDEV(major, minor + i));
        ++i;
    }
	class_destroy(devclass);

 	if (cdev) {
 		cdev_del(cdev);
 	}
 
 	if (vscdd_buffer) 
 		kfree(vscdd_buffer);
  	
 	// Освобождаем старшие и младшие номера устройств.
 	unregister_chrdev_region(devno, count);
 
 	pr_info("=== vscdd: exit ===\n");
}
 
/* 
 * Функция загрузки модуля и инициализации драйвера
 */
 static int __init vscdd_init(void)
{
    int result;  
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
        pr_info("=== vscdd: can't get a majori ===\n");
        return result;  
    }
 
    cdev = cdev_alloc();
    if (!cdev) {
        result = -ENOMEM;
        goto fail; 
    }

    cdev_init(cdev, &vscdd_fops);
    cdev->owner = THIS_MODULE;
    if (cdev_add(cdev, dev, count)) { 
        pr_info("=== vscdd: cdev_add error ===\n");
    }
    pr_info( "=== vscdd: %d:%d ===\n", major, minor);
 
    devclass = class_create(THIS_MODULE, "dev_class");
    int i = 0;
    while(i < count)
    {       
        #define DEVNAME "vscdd"
            dev = MKDEV(major, minor + i);
        #ifdef LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
            device_create(devclass, NULL, dev, "%s_%d", DEVNAME, i);
        #else
            device_create(devclass, NULL, dev, NULL, "%s_%d", DEVNAME, i);
        #endif  
    }
    pr_info("==module installed %d:[%d-%d]==\n", MAJOR(dev), minor, MINOR(dev)); 
    
    vscdd_buffer = kzalloc(100 * sizeof (*vscdd_buffer), GFP_KERNEL);    
    if (!vscdd_buffer) {
        result = -ENOMEM;
        goto fail;
    }
    
    return 0;
    
    fail:
        vscdd_exit();
        pr_info("=== vscdd: register error ===\n");
        return result; 
}
 
/*
 * Функции загрузки и выгрузки модуля
 */
module_init(vscdd_init);
module_exit(vscdd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Артамонова А.");
MODULE_DESCRIPTION("Шаблон для разработки драйвера символьного устройства");
MODULE_VERSION("20160525);