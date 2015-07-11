#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <asm/uaccess.h>

#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>

/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
#define SPIDEV_MAJOR			153	/* assigned */
#define N_SPI_MINORS			32	/* ... up to 256 */

static DECLARE_BITMAP(minors, N_SPI_MINORS);


/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for some settings can cause *lots* of trouble for other
 * devices on a shared bus:
 *
 *  - CS_HIGH ... this device will be active when it shouldn't be
 *  - 3WIRE ... when active, it won't behave as it should
 *  - NO_CS ... there will be no explicit message boundaries; this
 *	is completely incompatible with the shared bus model
 *  - READY ... transfers may proceed when they shouldn't.
 *
 * REVISIT should changing those flags be privileged?
 */
#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
				| SPI_NO_CS | SPI_READY)

struct rc522_data {
	dev_t			devt;
	spinlock_t		spi_lock;
	struct spi_device	*spi;
	struct list_head	device_entry;

	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex		buf_lock;
	unsigned		users;
	u8			*buffer;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

/*-------------------------------------------------------------------------*/

#define RC522_RESET_PIN	EXYNOS4_GPK1(0)

void rc522_reset()
{
	//printk("************************ %s\n", __FUNCTION__);
	if(gpio_request_one(RC522_RESET_PIN, GPIOF_OUT_INIT_HIGH, "RC522_RESET"))
                pr_err("failed to request GPK1_0 for RC522 reset control\n");

        s3c_gpio_setpull(RC522_RESET_PIN, S3C_GPIO_PULL_UP);
        gpio_set_value(RC522_RESET_PIN, 0);

        mdelay(5);

        gpio_set_value(RC522_RESET_PIN, 1);
        gpio_free(RC522_RESET_PIN);
}

/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */
static void rc522_complete(void *arg)
{
	complete(arg);
}

static ssize_t
rc522_sync(struct rc522_data *rc522, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;

	message->complete = rc522_complete;
	message->context = &done;

	spin_lock_irq(&rc522->spi_lock);
	if (rc522->spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_async(rc522->spi, message);
	spin_unlock_irq(&rc522->spi_lock);

	if (status == 0) {
		wait_for_completion(&done);
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static inline ssize_t
rc522_sync_write(struct rc522_data *rc522, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= rc522->buffer,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return rc522_sync(rc522, &m);
}

static inline ssize_t
rc522_sync_read(struct rc522_data *rc522, size_t len)
{
	struct spi_transfer	t = {
			.rx_buf		= rc522->buffer,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return rc522_sync(rc522, &m);
}

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t
rc522_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct rc522_data	*rc522;
	ssize_t			status = 0;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	rc522 = filp->private_data;

	mutex_lock(&rc522->buf_lock);
	status = rc522_sync_read(rc522, count);
	if (status > 0) {
		unsigned long	missing;

		missing = copy_to_user(buf, rc522->buffer, status);
		if (missing == status)
			status = -EFAULT;
		else
			status = status - missing;
	}
	mutex_unlock(&rc522->buf_lock);

	return status;
}

/* Write-only message with current device setup */
static ssize_t
rc522_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct rc522_data	*rc522;
	ssize_t			status = 0;
	unsigned long		missing;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	rc522 = filp->private_data;

	mutex_lock(&rc522->buf_lock);
	missing = copy_from_user(rc522->buffer, buf, count);
	if (missing == 0) {
		status = rc522_sync_write(rc522, count);
	} else
		status = -EFAULT;
	mutex_unlock(&rc522->buf_lock);

	return status;
}

static int rc522_message(struct rc522_data *rc522,
		struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
	struct spi_message	msg;
	struct spi_transfer	*k_xfers;
	struct spi_transfer	*k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned		n, total;
	u8			*buf;
	int			status = -EFAULT;

	spi_message_init(&msg);
	k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
	if (k_xfers == NULL)
		return -ENOMEM;

	/* Construct spi_message, copying any tx data to bounce buffer.
	 * We walk the array of user-provided transfers, using each one
	 * to initialize a kernel version of the same transfer.
	 */
	buf = rc522->buffer;
	total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		k_tmp->len = u_tmp->len;

		total += k_tmp->len;
		if (total > bufsiz) {
			status = -EMSGSIZE;
			goto done;
		}

		if (u_tmp->rx_buf) {
			k_tmp->rx_buf = buf;
			if (!access_ok(VERIFY_WRITE, (u8 __user *)
						(uintptr_t) u_tmp->rx_buf,
						u_tmp->len))
				goto done;
		}
		if (u_tmp->tx_buf) {
			k_tmp->tx_buf = buf;
			if (copy_from_user(buf, (const u8 __user *)
						(uintptr_t) u_tmp->tx_buf,
					u_tmp->len))
				goto done;
		}
		buf += k_tmp->len;

		k_tmp->cs_change = !!u_tmp->cs_change;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay_usecs = u_tmp->delay_usecs;
		k_tmp->speed_hz = u_tmp->speed_hz;
#ifdef VERBOSE
		dev_dbg(&rc522->spi->dev,
			"  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
			u_tmp->len,
			u_tmp->rx_buf ? "rx " : "",
			u_tmp->tx_buf ? "tx " : "",
			u_tmp->cs_change ? "cs " : "",
			u_tmp->bits_per_word ? : rc522->spi->bits_per_word,
			u_tmp->delay_usecs,
			u_tmp->speed_hz ? : rc522->spi->max_speed_hz);
#endif
		spi_message_add_tail(k_tmp, &msg);
	}

	status = rc522_sync(rc522, &msg);
	if (status < 0)
		goto done;

	/* copy any rx data out of bounce buffer */
	buf = rc522->buffer;
	for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
		if (u_tmp->rx_buf) {
			if (__copy_to_user((u8 __user *)
					(uintptr_t) u_tmp->rx_buf, buf,
					u_tmp->len)) {
				status = -EFAULT;
				goto done;
			}
		}
		buf += u_tmp->len;
	}
	status = total;

done:
	kfree(k_xfers);
	return status;
}

static long
rc522_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			err = 0;
	int			retval = 0;
	struct rc522_data	*rc522;
	struct spi_device	*spi;
	u32			tmp;
	unsigned		n_ioc;
	struct spi_ioc_transfer	*ioc;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	rc522 = filp->private_data;
	spin_lock_irq(&rc522->spi_lock);
	spi = spi_dev_get(rc522->spi);
	spin_unlock_irq(&rc522->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* use the buffer lock here for triple duty:
	 *  - prevent I/O (from us) so calling spi_setup() is safe;
	 *  - prevent concurrent SPI_IOC_WR_* from morphing
	 *    data fields while SPI_IOC_RD_* reads them;
	 *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
	 */
	mutex_lock(&rc522->buf_lock);

	switch (cmd) {
	/* read requests */
	case SPI_IOC_RD_MODE:
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_LSB_FIRST:
		retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_BITS_PER_WORD:
		retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MAX_SPEED_HZ:
		retval = __put_user(spi->max_speed_hz, (__u32 __user *)arg);
		break;

	/* write requests */
	case SPI_IOC_WR_MODE:
		retval = __get_user(tmp, (u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->mode;

			if (tmp & ~SPI_MODE_MASK) {
				retval = -EINVAL;
				break;
			}

			tmp |= spi->mode & ~SPI_MODE_MASK;
			spi->mode = (u8)tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "spi mode %02x\n", tmp);
		}
		break;
	case SPI_IOC_WR_LSB_FIRST:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->mode;

			if (tmp)
				spi->mode |= SPI_LSB_FIRST;
			else
				spi->mode &= ~SPI_LSB_FIRST;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "%csb first\n",
						tmp ? 'l' : 'm');
		}
		break;
	case SPI_IOC_WR_BITS_PER_WORD:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->bits_per_word;

			spi->bits_per_word = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->bits_per_word = save;
			else
				dev_dbg(&spi->dev, "%d bits per word\n", tmp);
		}
		break;
	case SPI_IOC_WR_MAX_SPEED_HZ:
		retval = __get_user(tmp, (__u32 __user *)arg);
		if (retval == 0) {
			u32	save = spi->max_speed_hz;

			spi->max_speed_hz = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->max_speed_hz = save;
			else
				dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
		}
		break;

	default:
		/* segmented and/or full-duplex I/O request */
		if (_IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
				|| _IOC_DIR(cmd) != _IOC_WRITE) {
			retval = -ENOTTY;
			break;
		}

		tmp = _IOC_SIZE(cmd);
		if ((tmp % sizeof(struct spi_ioc_transfer)) != 0) {
			retval = -EINVAL;
			break;
		}
		n_ioc = tmp / sizeof(struct spi_ioc_transfer);
		if (n_ioc == 0)
			break;

		/* copy into scratch area */
		ioc = kmalloc(tmp, GFP_KERNEL);
		if (!ioc) {
			retval = -ENOMEM;
			break;
		}
		if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
			kfree(ioc);
			retval = -EFAULT;
			break;
		}

		/* translate to spi_message, execute */
		retval = rc522_message(rc522, ioc, n_ioc);
		kfree(ioc);
		break;
	}

	mutex_unlock(&rc522->buf_lock);
	spi_dev_put(spi);
	return retval;
}

#ifdef CONFIG_COMPAT
static long
rc522_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return rc522_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define rc522_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int rc522_open(struct inode *inode, struct file *filp)
{
	struct rc522_data	*rc522;
	int			status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(rc522, &device_list, device_entry) {
		if (rc522->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (!rc522->buffer) {
			rc522->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!rc522->buffer) {
				dev_dbg(&rc522->spi->dev, "open/ENOMEM\n");
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			rc522->users++;
			filp->private_data = rc522;
			nonseekable_open(inode, filp);
		}
	} else
		pr_debug("spidev: nothing for minor %d\n", iminor(inode));

	mutex_unlock(&device_list_lock);
	return status;
}

static int rc522_release(struct inode *inode, struct file *filp)
{
	struct rc522_data	*rc522;
	int status = 0;

	mutex_lock(&device_list_lock);
	rc522 = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	rc522->users--;
	if (!rc522->users) {
		int		dofree;

		kfree(rc522->buffer);
		rc522->buffer = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&rc522->spi_lock);
		dofree = (rc522->spi == NULL);
		spin_unlock_irq(&rc522->spi_lock);

		if (dofree)
			kfree(rc522);
	}
	mutex_unlock(&device_list_lock);

	return status;
}

static const struct file_operations rc522_fops = {
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.write = rc522_write,
	.read = rc522_read,
	.unlocked_ioctl = rc522_ioctl,
	.compat_ioctl = rc522_compat_ioctl,
	.open = rc522_open,
	.release = rc522_release,
	.llseek =	no_llseek,
};

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *rc522_class;

/*-------------------------------------------------------------------------*/

static int __devinit rc522_probe(struct spi_device *spi)
{
	struct rc522_data *rc522;
	int status;
	unsigned long minor;

	/* Allocate driver data */
	rc522 = kzalloc(sizeof(*rc522), GFP_KERNEL);
	if (!rc522)
		return -ENOMEM;

	/* reset */
	rc522_reset();

	/* Initialize the driver data */
	rc522->spi = spi;
	spin_lock_init(&rc522->spi_lock);
	mutex_init(&rc522->buf_lock);

	INIT_LIST_HEAD(&rc522->device_entry);

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		rc522->devt = MKDEV(SPIDEV_MAJOR, minor);

		dev = device_create(rc522_class, &spi->dev, rc522->devt,
				    rc522, "rc522",
				    spi->master->bus_num, spi->chip_select);

		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&rc522->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	if (status == 0)
		spi_set_drvdata(spi, rc522);
	else
		kfree(rc522);

	return status;
}

static int __devexit rc522_remove(struct spi_device *spi)
{
	struct rc522_data	*rc522 = spi_get_drvdata(spi);

	/* make sure ops on existing fds can abort cleanly */
	spin_lock_irq(&rc522->spi_lock);
	rc522->spi = NULL;
	spi_set_drvdata(spi, NULL);
	spin_unlock_irq(&rc522->spi_lock);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&rc522->device_entry);
	device_destroy(rc522_class, rc522->devt);
	clear_bit(MINOR(rc522->devt), minors);
	if (rc522->users == 0)
		kfree(rc522);
	mutex_unlock(&device_list_lock);

	return 0;
}

static struct spi_driver rc522_spi_driver = {
	.driver = {
		.name =	"rc522",
		.owner =	THIS_MODULE,
	},
	.probe =	rc522_probe,
	.remove = __devexit_p(rc522_remove),

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};

/*-------------------------------------------------------------------------*/

static int __init rc522_init(void)
{
	int status;

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(SPIDEV_MAJOR, "rc522_test", &rc522_fops);
	if (status < 0)
		return status;

	rc522_class = class_create(THIS_MODULE, "rc522");
	if (IS_ERR(rc522_class)) {
		unregister_chrdev(SPIDEV_MAJOR, rc522_spi_driver.driver.name);
		return PTR_ERR(rc522_class);
	}

	status = spi_register_driver(&rc522_spi_driver);
	if (status < 0) {
		class_destroy(rc522_class);
		unregister_chrdev(SPIDEV_MAJOR, rc522_spi_driver.driver.name);
	}
	return status;
}
module_init(rc522_init);

static void __exit rc522_exit(void)
{
	spi_unregister_driver(&rc522_spi_driver);
	class_destroy(rc522_class);
	unregister_chrdev(SPIDEV_MAJOR, rc522_spi_driver.driver.name);
}
module_exit(rc522_exit);

MODULE_AUTHOR("topeet: cym");
MODULE_LICENSE("GPL");
//MODULE_ALIAS("spi:spidev");
