/*
 * Шаблон для разработки драйвера символьного устройства
 * vscdd.c -- very simple chrdev driver
 */

#include <linux/types.h>        /* dev_t */
  #include <linux/cdev.h>
  #include <asm/uaccess.h>        /* copy_*_user */
 +#include <linux/version.h> 
 +#include <linux/device.h>
  
  #ifndef MODULE_NAME
  #define MODULE_NAME "vscdd"
 @@ -33,11 +35,18 @@ module_param(count, int, S_IRUGO);
   */
  static int device_open = 0;
  
 +/*
 +* Размер файла (для llseek)
 +*/
 +static int cur_size = 0;
 +
  /* 
   * Структура драйвера символьного устройства
   */ 
  struct cdev *cdev;
  
 +static struct class *devclass;
 +
  /* 
   * Память устройства
   */
 @@ -48,6 +57,12 @@ static char *vscdd_buffer;
   */
  int vscdd_open(struct inode *inode, struct file *filp)
  {
 +	if (device_open) {
 +		pr_info("=== vscdd: device is already opened ===\n");
 +		return -EBUSY;
 +	}
 +	device_open++;
 +	pr_info("=== vscdd: opening device ===\n");
  	return 0;
  }
  
 @@ -56,9 +71,42 @@ int vscdd_open(struct inode *inode, struct file *filp)
   */
  int vscdd_release(struct inode *inode, struct file *filp)
  {
 +	if (!device_open) return -EFAULT;
 +	device_open--;
  	return 0;
  }
 +/*
 + * Функция перемещения указателя чтения-записи
 + */
 +loff_t vscdd_llseek(struct file *filp, loff_t offset, int origin)
 +{
 +  loff_t new_pos;
 +
 +  pr_info("=== vscdd: f_pos = %d ===\n", filp->f_pos); 
 +  pr_info("=== vscdd: offset = %d ===\n", offset);
 + 
 +  switch(origin) {
 +   case 0: 
 +    new_pos = offset;
 +    break;
  
 +   case 1: 
 +    new_pos = filp->f_pos + offset;
 +    break;
 +
 +   case 2: 
 +    new_pos = cur_size + offset;
 +    break;
 +
 +   default:
 +    return -EINVAL;
 +  }
 +  pr_info("=== vscdd: new_pos = %d===\n", new_pos);
 +  
 +  if (new_pos<0) return -EINVAL;
 +  filp->f_pos = new_pos;
 +  return new_pos;
 +}
  /*
   * Функция чтения из устройства
   */
 @@ -107,6 +155,7 @@ ssize_t vscdd_write(struct file *filp, const char __user *buf, size_t count, lof
  	}
  	// увеличить значение указателя
  	*f_pos += count;
 +	cur_size += count;
  	retval = count;
  
    out:
 @@ -118,7 +167,7 @@ ssize_t vscdd_write(struct file *filp, const char __user *buf, size_t count, lof
   */
  struct file_operations vscdd_fops = {
  	.owner =    THIS_MODULE,
 -	//.llseek =   llseek,
 +	.llseek =   vscdd_llseek,
  	.read =     vscdd_read,
  	.write =    vscdd_write,
  	.open =     vscdd_open,
 @@ -130,7 +179,15 @@ struct file_operations vscdd_fops = {
   */
  static void __exit vscdd_exit(void) 
  {
 +	dev_t dev;
  	dev_t devno = MKDEV(major, minor);
 +   	int i;
 +   	for( i = 0; i < count; i++ ) {
 +      		dev = MKDEV( major, minor + i );
 +      		device_destroy( devclass, dev );
 +   	}
 +    	class_destroy( devclass );
 +	
  	if (cdev) {
  		cdev_del(cdev);
  	}
 @@ -149,6 +206,7 @@ static void __exit vscdd_exit(void)
   */
  static int __init vscdd_init(void)
  {
 +	int i;
  	int result;
  	dev_t dev = 0;
  	result = 0;
 @@ -180,6 +238,21 @@ static int __init vscdd_init(void)
  		pr_info("=== vscdd: cdev_add error ===\n");
  	}
  	pr_info( "=== vscdd: %d:%d ===\n", major, minor);
 +	
 +	devclass = class_create( THIS_MODULE, "vscdd_class" ); 
 +   	
 +   	for( i = 0; i < count; i++ ) {
 +#define DEVNAME "vscdd"
 +      	dev = MKDEV( major, minor + i );
 +#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
 +
 +      device_create( devclass, NULL, dev, "%s_%d", DEVNAME, i );
 +#else
 +
 +      device_create( devclass, NULL, dev, NULL, "%s_%d", DEVNAME, i );
 +#endif
 +   }
 +   	pr_info( "=== vscdd: module installed %d:[%d-%d] ===\n", MAJOR(dev), minor, MINOR(dev) ); 
  
  	vscdd_buffer = kzalloc(100 * sizeof (*vscdd_buffer), GFP_KERNEL);
  	if (!vscdd_buffer) {
 @@ -202,7 +275,7 @@ static int __init vscdd_init(void)
  module_init(vscdd_init);
  module_exit(vscdd_exit);
  
 -MODULE_LICENSE("DUAL BSD/GPL");
 +MODULE_LICENSE("GPL");
  MODULE_AUTHOR ("МИФИ");
 -MODULE_DESCRIPTION("Шаблон для разработки драйвера символьного устройства");
 -MODULE_VERSION("20160503");
 +MODULE_DESCRIPTION("Шаблон для разработки драйвера символьного устройства, дополнен Медниковым В.Д.");
 +MODULE_VERSION("20160524");
