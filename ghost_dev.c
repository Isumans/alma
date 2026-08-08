#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

/* NOTE: <linux/device.h> is intentionally omitted.
 * class_create(), device_create(), class_destroy(), device_destroy(), and
 * class_unregister() are all EXPORT_SYMBOL_GPL and cannot be used by a
 * MODULE_LICENSE("Proprietary") module (enforced by modpost since kernel 5.9).
 * The device node must be created manually after insmod — see instructions below. */

#define DEVICE_NAME "ghost_dev"
#define BUF_SIZE 256

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Ghost character device driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("Proprietary");

static int major;
static char kernel_buffer[BUF_SIZE] = "I am a ghost in the kernel...\n";

static DEFINE_MUTEX(ghost_mutex);

/* ── open / release ─────────────────────────────────────────────────── */

static int dev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "GhostDev: Opened\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "GhostDev: Released\n");
    return 0;
}

/* ── read ────────────────────────────────────────────────────────────── */

static ssize_t dev_read(struct file *file, char __user *buf,
                        size_t len, loff_t *off)
{
    int bytes_to_read;
    int ret = 0;

    mutex_lock(&ghost_mutex);

    bytes_to_read = strlen(kernel_buffer);
    if (*off >= bytes_to_read)
        goto out;

    if (copy_to_user(buf, kernel_buffer, bytes_to_read)) {
        ret = -EFAULT;
        goto out;
    }

    *off += bytes_to_read;
    ret = bytes_to_read;

out:
    mutex_unlock(&ghost_mutex);
    return ret;
}

/* ── write ───────────────────────────────────────────────────────────── */

static ssize_t dev_write(struct file *file, const char __user *buf,
                         size_t len, loff_t *off)
{
    size_t amount;
    int ret;

    mutex_lock(&ghost_mutex);

    amount = min(len, (size_t)BUF_SIZE - 1);

    if (copy_from_user(kernel_buffer, buf, amount)) {
        ret = -EFAULT;
        goto out;
    }

    kernel_buffer[amount] = '\0';
    ret = amount;   /* return only what was actually written */

out:
    mutex_unlock(&ghost_mutex);
    return ret;
}

/* ── file operations ─────────────────────────────────────────────────── */

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .read    = dev_read,
    .write   = dev_write,
};

/* ── init / exit ─────────────────────────────────────────────────────── */

static int __init ghost_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0)
        return major;

    printk(KERN_INFO "GhostDev: Registered with major %d\n", major);
    printk(KERN_INFO "GhostDev: Run -> mknod /dev/%s c %d 0\n", DEVICE_NAME, major);
    return 0;
}

static void __exit ghost_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "GhostDev: Unregistered\n");
    printk(KERN_INFO "GhostDev: Run -> rm /dev/%s\n", DEVICE_NAME);
}

module_init(ghost_init);
module_exit(ghost_exit);
