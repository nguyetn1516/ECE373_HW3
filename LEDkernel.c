/* 
LEDkernel.c

Description: This is the kernel space file for ECE 373 Assignment 3.
			PCI deiver is hooked up into init_module() and unregistered
			in exit_module()
			
Natalie Nguyen
ECE 373 - Assignment 3
05/05/2020
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pci.h>
#include <linux/netdevice.h>

#define DEVCNT 1
#define DEVNAME "LEDdriver"

int init_syscall_val = 40;
static struct mydev_dev {
	struct cdev my_cdev;
	dev_t mydev_node;
	int syscall_val;
	long initialVal_LED;
} mydev;

static struct PCI {
	struct pci_dev *pdev;
	void *hw_adder;		
} PCI;


module_param(init_syscall_val, int, S_IRUSR | S_IWUSR);		// make param readable and writeable to user
char LEDdriverName[] = "LEDdriver";


static const struct pci_device_id LEDdriver_table[] = {
	{ PCI_DEVICE(0x8086, 0x100e)},						// Vendor ID, PCI device #
	{},
};


static int LEDdriver_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "successfully opened!\n");
	return 0;
}


/****************************************************** READ OPERATIONS ********************************************/
static ssize_t LEDdriver_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	/* Get a local kernel buffer set aside */
	int ret;

	/* Make sure our user wasn't bad... */
	if (!buf) {
		ret = -EINVAL;
		goto out;
	}

	mydev.syscall_val = readl(PCI.hw_adder + 0xE00);				// Store hw_adder value in syscall_val

	if (copy_to_user(buf, &mydev.syscall_val, sizeof(int))) {
		ret = -EFAULT;
		goto out;
	}
	ret = sizeof(int);
	*offset += sizeof(int);

	/* Good to go, so printk the thingy */
	printk(KERN_INFO "User got from us %x\n", mydev.syscall_val);

out:
	return ret;
}


/****************************************************** WRITE OPERATIONS ********************************************/
static ssize_t LEDdriver_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
	/* Have local kernel memory ready */
	char *kern_buf;
	int ret;

	/* Make sure our user isn't bad... */
	if (!buf) {
		ret = -EINVAL;
		goto out;
	}

	/* Get some memory to copy into... */
	kern_buf = kmalloc(len, GFP_KERNEL);

	/* ...and make sure it's good to go */
	if (!kern_buf) {
		ret = -ENOMEM;
		goto out;
	}
	
	/* Copy from the user-provided buffer */
	if (copy_from_user(kern_buf, buf, len)) {
		/* uh-oh... */
		ret = -EFAULT;
		goto mem_out;
	}


	/* kern_buf = 32 bits and write to LED */
	writel(*((u32*)kern_buf), PCI.hw_adder + 0xE00);		
	ret = len;

	/* print what userspace gave us */
	printk(KERN_INFO "Userspace wrote \"%x\" to us\n", mydev.syscall_val);

mem_out:
	kfree(kern_buf);
out:
	return ret;
}


/****************************************************** PROBE FUNCTION ********************************************/
static int LEDdiver_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	resource_size_t mmio_start, mmio_len;		// memory I/O variables
	unsigned long BARmask;

	BARmask = pci_select_bars(pdev, IORESOURCE_MEM);
	printk(KERN_INFO "BAR mask %lx", BARmask);

	if(pci_request_selected_regions(pdev, BARmask, LEDdriverName))
	{
		printk(KERN_ERR "Request selected regions failed\n");
		pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
		return -1;
	}

	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev, 0);

	printk(KERN_INFO "mmio_start: %lx\n", (unsigned long) mmio_start);
	printk(KERN_INFO "mmio_len: %lx\n", (unsigned long) mmio_len);

	if (!(PCI.hw_adder = ioremap(mmio_start, mmio_len)))
	{
		printk(KERN_INFO "ioremap failed\n");
		iounmap(PCI.hw_adder);
		return -1;
	}

	mydev.initialVal_LED = readl(PCI.hw_adder + 0xE00);
	printk(KERN_INFO "LED initial value: %lx\n", mydev.initialVal_LED);
	return 0;
}


/****************************************************** REMOVE FUNCTION ********************************************/
static void LEDdriver_remove(struct pci_dev *pdev)
{
	struct PCI *temp = pci_get_drvdata(pdev);
	iounmap(temp -> hw_adder);
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
}



/* File operations for our device */
static struct file_operations mydev_fops = {
	.owner = THIS_MODULE,
	.open = LEDdriver_open,
	.read = LEDdriver_read,
	.write = LEDdriver_write,
};

static struct pci_driver LEDdriver = 
{
	.name = "LED driver",
	.id_table = LEDdriver_table,
	.probe = LEDdiver_probe,
	.remove = LEDdriver_remove,
};


/****************************************************** MODULE INIT **********************************************/
static int __init LEDdriver_init(void)
{
	printk(KERN_INFO "LEDdriver module loading...\n");

	if (alloc_chrdev_region(&mydev.mydev_node, 0, DEVCNT, DEVNAME)) 
	{
		printk(KERN_ERR "alloc_chrdev_region() failed!\n");
		return -1;
	}

	printk(KERN_INFO "Allocated %d devices at major: %d\n", DEVCNT, MAJOR(mydev.mydev_node));

	/* Initialize the character device and add it to the kernel */
	cdev_init(&mydev.my_cdev, &mydev_fops);
	mydev.my_cdev.owner = THIS_MODULE;

	if (cdev_add(&mydev.my_cdev, mydev.mydev_node, DEVCNT)) {
		printk(KERN_ERR "cdev_add() failed!\n");
		/* clean up chrdev allocation */
		unregister_chrdev_region(mydev.mydev_node, DEVCNT);

		return -1;
	}

	if(pci_register_driver(&LEDdriver))
	{
		printk(KERN_ERR "pci_register_driver failed\n");
		pci_unregister_driver(&LEDdriver);
		return -1;
	}

	return 0;
}


/****************************************************** MODULE EXIT ************************************************/
static void __exit LEDdriver_exit(void)
{
	/* destroy the cdev */
	cdev_del(&mydev.my_cdev);

	/* clean up the devices */
	unregister_chrdev_region(mydev.mydev_node, DEVCNT);

	/* unregister pci_evice struct */
	pci_unregister_driver(&LEDdriver);
	printk(KERN_INFO "deviceNode module unloaded!\n");
}

MODULE_AUTHOR("Natalie Nguyen");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
module_init(LEDdriver_init);
module_exit(LEDdriver_exit);
