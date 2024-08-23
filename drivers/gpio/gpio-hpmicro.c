// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for hpm_gpio, pca857x, and pca967x I2C GPIO expanders
 *
 * Copyright (C) 2007 David Brownell
 */

#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#define HPM_DI_OFFSET  0x00
#define HPM_DO_OFFSET  0x100
#define HPM_SET_OFFSET 0x104
#define HPM_CLR_OFFSET 0x108
#define HPM_TOGGLE_OFFSET  0x10C

static const struct i2c_device_id hpm_gpio_id[] = {
	{ "hpmicro", 8 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hpm_gpio_id);

static const struct of_device_id hpm_gpio_of_table[] = {
	{ .compatible = "hpmicro,gpio", (void *)8 },
	{ }
};
MODULE_DEVICE_TABLE(of, hpm_gpio_of_table);

/*
 * The hpm_gpio, pca857x, and pca967x chips only expose one read and one
 * write register.  Writing a "one" bit (to match the reset state) lets
 * that pin be used as an input; it's not an open-drain model, but acts
 * a bit like one.  This is described as "quasi-bidirectional"; read the
 * chip documentation for details.
 *
 * Many other I2C GPIO expander chips (like the pca953x models) have
 * more complex register models and more conventional circuitry using
 * push/pull drivers.  They often use the same 0x20..0x27 addresses as
 * hpm_gpio parts, making the "legacy" I2C driver model problematic.
 */

struct hpm_gpio_port {
	char *name;
	unsigned int base;
};

struct hpm_gpio {
	unsigned int        *base;
	struct gpio_chip	**chip;
	struct hpm_gpio_port **chip_data;
	unsigned int	*dev;
	unsigned int		out;		/* software latch */
	unsigned int		status;		/* current status */
	unsigned int		irq_enabled;	/* enabled irqs */
};

const char * const gpio_port_names[] = {
	"HPMICRO GPIOA",
	"HPMICRO GPIOB",
	"HPMICRO GPIOC",
	"HPMICRO GPIOD",
	"HPMICRO GPIOE",
	"HPMICRO GPIOF",
	"HPMICRO GPIOG",
	"HPMICRO GPIOY",
	"HPMICRO GPIOZ",
};

/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/

static int hpm_gpio_input(struct gpio_chip *chip, unsigned int offset)
{
	struct hpm_gpio_port *port = gpiochip_get_data(chip);
	int status;
	printk("%s %d base: 0x%08x %d\n", __func__, __LINE__, port->base, offset);

	return status;
}

static int hpm_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct hpm_gpio_port *port = gpiochip_get_data(chip);
	int value;
	printk("%s %d 0x%08x base: 0x%08x %d\n", __func__, __LINE__, chip, port->base, offset);

	value = *(unsigned int *)(port->base + HPM_DI_OFFSET) & (1 << offset);
	return (value == 0) ? 0 : 1;
}

static int hpm_gpio_get_multiple(struct gpio_chip *chip, unsigned long *mask,
				unsigned long *bits)
{
	struct hpm_gpio_port *port = gpiochip_get_data(chip);
	int value = *(unsigned int *)(port->base + HPM_DI_OFFSET);

	printk("%s %d base: 0x%08x %d\n", __func__, __LINE__, port->base, *mask);
	if (value < 0)
		return value;

	*bits &= ~*mask;
	*bits |= value & *mask;

	return 0;
}

static int hpm_gpio_output(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct hpm_gpio_port *port = gpiochip_get_data(chip);
	printk("%s %d base: 0x%08x %d\n", __func__, __LINE__, port->base, offset);
	struct hpm_gpio *gpio = gpiochip_get_data(chip);
	unsigned int bit = 1 << offset;
	int status;

	if (value)
		*(unsigned int *)(port->base + HPM_SET_OFFSET) = bit;
	else
		*(unsigned int *)(port->base + HPM_CLR_OFFSET) = bit;

	return 0;
}

static void hpm_gpio_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct hpm_gpio_port *port = gpiochip_get_data(chip);
	printk("%s %d base: 0x%08x %d\n", __func__, __LINE__, port->base, offset);
	hpm_gpio_output(chip, offset, value);
}

static void hpm_gpio_set_multiple(struct gpio_chip *chip, unsigned long *mask,
				 unsigned long *bits)
{
	struct hpm_gpio_port *port = gpiochip_get_data(chip);
	printk("%s %d base: 0x%08x %d\n", __func__, __LINE__, port->base, *mask);
	*(unsigned int *)(port->base + HPM_SET_OFFSET) = ((*bits) & (*mask));
}

/*-------------------------------------------------------------------------*/

static irqreturn_t hpm_gpio_irq(int irq, void *data)
{
	// for_each_set_bit(i, &change, gpio->chip.ngpio)
	// 	handle_nested_irq(irq_find_mapping(gpio->chip.irq.domain, i));

	return IRQ_HANDLED;
}

/*
 * NOP functions
 */
static void noop(struct irq_data *data) { }

static int hpm_gpio_irq_set_wake(struct irq_data *data, unsigned int on)
{
	printk("%s %d\n", __func__, __LINE__);
	struct hpm_gpio *gpio = irq_data_get_irq_chip_data(data);

	// return irq_set_irq_wake(gpio->client->irq, on);
	return 0;
}

static void hpm_gpio_irq_enable(struct irq_data *data)
{
	printk("%s %d\n", __func__, __LINE__);
	// struct hpm_gpio *gpio = irq_data_get_irq_chip_data(data);
	// irq_hw_number_t hwirq = irqd_to_hwirq(data);

	// gpiochip_enable_irq(&gpio->chip, hwirq);
	// gpio->irq_enabled |= (1 << hwirq);
}

static void hpm_gpio_irq_disable(struct irq_data *data)
{
	printk("%s %d\n", __func__, __LINE__);
	// struct hpm_gpio *gpio = irq_data_get_irq_chip_data(data);
	// irq_hw_number_t hwirq = irqd_to_hwirq(data);

	// gpio->irq_enabled &= ~(1 << hwirq);
	// gpiochip_disable_irq(&gpio->chip, hwirq);
}

static void hpm_gpio_irq_bus_lock(struct irq_data *data)
{
	printk("%s %d\n", __func__, __LINE__);
	// struct hpm_gpio *gpio = irq_data_get_irq_chip_data(data);
}

static void hpm_gpio_irq_bus_sync_unlock(struct irq_data *data)
{
	printk("%s %d\n", __func__, __LINE__);
	// struct hpm_gpio *gpio = irq_data_get_irq_chip_data(data);
}

static const struct irq_chip hpm_gpio_irq_chip = {
	.name			= "hpm_gpio",
	.irq_enable		= hpm_gpio_irq_enable,
	.irq_disable		= hpm_gpio_irq_disable,
	.irq_ack		= noop,
	.irq_mask		= noop,
	.irq_unmask		= noop,
	.irq_set_wake		= hpm_gpio_irq_set_wake,
	.irq_bus_lock		= hpm_gpio_irq_bus_lock,
	.irq_bus_sync_unlock	= hpm_gpio_irq_bus_sync_unlock,
	.flags			= IRQCHIP_IMMUTABLE,
	GPIOCHIP_IRQ_RESOURCE_HELPERS,
};

/*-------------------------------------------------------------------------*/

static int hpm_gpio_probe(struct platform_device *pdev)
{
	struct hpm_gpio *gpio;
	struct gpio_chip *chip;
	unsigned int n_latch = 0;
	unsigned int n_ports = 0;
	int status;
	printk("%s %d\n", __func__, __LINE__);
	device_property_read_u32(&pdev->dev, "lines-initial-states", &n_latch);
	device_property_read_u32(&pdev->dev, "n-ports", &n_ports);

	/* Allocate, initialize, and register this gpio_chip. */
	gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio) {
		status = -ENOMEM;
		goto fail;
	}
	gpio->chip = devm_kzalloc(&pdev->dev, n_ports * sizeof(struct gpio_chip *), GFP_KERNEL);
	if (!gpio->chip) {
		status = -ENOMEM;
		goto fail;
	}
	gpio->chip_data = devm_kzalloc(&pdev->dev, n_ports * sizeof(struct hpm_gpio_port *), GFP_KERNEL);
	if (!gpio->chip_data) {
		status = -ENOMEM;
		goto fail;
	}
	gpio->base = devm_platform_ioremap_resource(pdev, 0);
	
	dev_info(&pdev->dev, "hpmicro gpio map result is  0x%08x\n", gpio->base);
	for (int i = 0; i < n_ports; i++) {
		chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
		if (!chip) {
			status = -ENOMEM;
			goto fail;
		}

		chip->base = -1;
		chip->can_sleep		= false;
		chip->parent		= &pdev->dev;
		chip->owner		= THIS_MODULE;
		chip->get			= hpm_gpio_get;
		chip->get_multiple		= hpm_gpio_get_multiple;
		chip->set			= hpm_gpio_set;
		chip->set_multiple		= hpm_gpio_set_multiple;
		chip->direction_input	= hpm_gpio_input;
		chip->direction_output	= hpm_gpio_output;
		chip->ngpio = 32;

		gpio->chip[i] = chip;
		gpio->chip[i]->label = pdev->name;
		gpio->chip_data[i] = devm_kzalloc(&pdev->dev, sizeof(struct hpm_gpio_port), GFP_KERNEL);
		if (!gpio->chip_data[i]) {
			status = -ENOMEM;
			goto fail;
		}
		gpio->chip_data[i]->base = (int)((uint)gpio->base + i * 0x10);
		dev_info(&pdev->dev, "add gpiochip data %d\n", i);
		status = devm_gpiochip_add_data(&pdev->dev, gpio->chip[i], gpio->chip_data[i]);
		if (status < 0)
			goto fail;
	}


	gpio->dev = (unsigned int *)pdev;
#if 0
	/* Enable irqchip if we have an interrupt */
	if (pdev->irq) {
		struct gpio_irq_chip *girq;

		status = devm_request_threaded_irq(&pdev->dev, pdev->irq,
					NULL, hpm_gpio_irq, IRQF_ONESHOT |
					IRQF_TRIGGER_FALLING | IRQF_SHARED,
					dev_name(&pdev->dev), gpio);
		if (status)
			goto fail;

		girq = &gpio->chip.irq;
		gpio_irq_chip_set_chip(girq, &hpm_gpio_irq_chip);
		/* This will let us handle the parent IRQ in the driver */
		girq->parent_handler = NULL;
		girq->num_parents = 0;
		girq->parents = NULL;
		girq->default_type = IRQ_TYPE_NONE;
		girq->handler = handle_level_irq;
		girq->threaded = true;
	}
#endif
	// for (int i = 0; i < n_ports; i++) {
	// 	dev_info(&pdev->dev, "add gpiochip %d\n", i);
	// 	status = devm_gpiochip_add_data(&pdev->dev, gpio->chip[i], gpio);
	// 	if (status < 0)
	// 		goto fail;
	// }

	dev_info(&pdev->dev, "probed\n");

	printk("%s %d\n", __func__, __LINE__);
	return 0;

fail:
	dev_dbg(&pdev->dev, "probe error %d for '%s'\n", status,
		pdev->name);
	if (gpio) {
		if (gpio->chip) {
			for (int j = 0; j < n_ports; j++) {
				if (gpio->chip[j]) {
					if (gpio->chip_data) {
						if (gpio->chip_data[j]) {
							devm_kfree(&pdev->dev, gpio->chip_data[j]);
						}
					}
					devm_kfree(&pdev->dev, gpio->chip[j]);
				}
			}
			if (gpio->chip_data) {
				devm_kfree(&pdev->dev, gpio->chip_data);
			}
			devm_kfree(&pdev->dev, gpio->chip);
		}
		devm_kfree(&pdev->dev, gpio);
	}
	return status;
}

static void hpm_gpio_shutdown(struct i2c_client *pdev)
{

	/* Drive all the I/O lines high */
	// gpio->write(gpio->pdev, BIT(gpio->chip.ngpio) - 1);
}

static struct platform_driver hpm_gpio_driver = {
	.driver		= {
		.name	= "hpm_gpio",
		.of_match_table = hpm_gpio_of_table,
	},
	.probe		= hpm_gpio_probe,
};
static int __init hpm_gpio_init(void)
{
	printk("123123\n");
	return platform_driver_register(&hpm_gpio_driver);
}
/* register after i2c postcore initcall and before
 * subsys initcalls that may rely on these GPIOs
 */
subsys_initcall(hpm_gpio_init);

static void __exit hpm_gpio_exit(void)
{
}
module_exit(hpm_gpio_exit);

MODULE_DESCRIPTION("Driver for hpm_gpio");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zihan XU");
