/*
 * Tiny Serial driver
 *
 * Copyright (C) 2002-2004 Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2 of the License.
 *
 * This driver shows how to create a minimal serial driver.  It does not rely on
 * any backing hardware, but creates a timer that emulates data being received
 * from some kind of hardware.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/module.h>


#define DRIVER_AUTHOR "Greg Kroah-Hartman <greg@kroah.com>"
#define DRIVER_DESC "Tiny serial driver"

/* Module information */
MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

#define DELAY_TIME		HZ * 2	/* 2 seconds per character */
#define TINY_DATA_CHARACTER	't'

#define TINY_SERIAL_MAJOR	241	/* experimental range */
#define TINY_SERIAL_MINORS	1	/* only have one minor */
#define UART_NR			1	/* only use one port */

#define TINY_SERIAL_NAME	"ttytiny"

#define MY_NAME			TINY_SERIAL_NAME

#define TINY_CLOCK		3672000

#define pr_enter() do { pr_notice("Enter: %s\n", __func__); } while (0)

static struct timer_list *timer;

static void tiny_stop_tx(struct uart_port *port)
{
	pr_enter();
}

static void tiny_stop_rx(struct uart_port *port)
{
	pr_enter();
}

static void tiny_enable_ms(struct uart_port *port)
{
	pr_enter();
}

static void tiny_tx_chars(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	int count;

	pr_enter();

	if (port->x_char) {
		pr_info("x_char wrote 0x%2x", port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		tiny_stop_tx(port);
		return;
	}

	count = port->fifosize >> 1;
	do {
		pr_info("xmit wrote 0x%2x", xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit))
		tiny_stop_tx(port);
}

static void tiny_start_tx(struct uart_port *port)
{
	pr_enter();
}

static void tiny_timer(unsigned long data)
{
	struct uart_port *port;
	struct tty_struct *tty;
	struct tty_port *tty_port;

	pr_enter();

	port = (struct uart_port *)data;
	if (!port)
		return;
	if (!port->state)
		return;
	tty = port->state->port.tty;
	if (!tty)
		return;

	tty_port = tty->port;

	/* add one character to the tty port */
	/* this doesn't actually push the data through unless tty->low_latency is set */
	tty_insert_flip_char(tty_port, TINY_DATA_CHARACTER, 0);

	tty_flip_buffer_push(tty_port);

	/* resubmit the timer again */
	timer->expires = jiffies + DELAY_TIME;
	add_timer(timer);

	/* see if we have any data to transmit */
	tiny_tx_chars(port);
}

static unsigned int tiny_tx_empty(struct uart_port *port)
{
	pr_enter();
	return 0;
}

static unsigned int tiny_get_mctrl(struct uart_port *port)
{
	pr_enter();
	return 0;
}

static void tiny_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	pr_enter();
}

static void tiny_break_ctl(struct uart_port *port, int break_state)
{
	pr_enter();
}

static void tiny_set_termios(struct uart_port *port,
			     struct ktermios *new, struct ktermios *old)
{
	int baud, quot, cflag = new->c_cflag;

	pr_enter();

	/* get the byte size */
	switch (cflag & CSIZE) {
	case CS5:
		printk(KERN_DEBUG " - data bits = 5\n");
		break;
	case CS6:
		printk(KERN_DEBUG " - data bits = 6\n");
		break;
	case CS7:
		printk(KERN_DEBUG " - data bits = 7\n");
		break;
	default: // CS8
		printk(KERN_DEBUG " - data bits = 8\n");
		break;
	}

	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			pr_info(" - parity = odd\n");
		else
			pr_info(" - parity = even\n");
	else
		pr_info(" - parity = none\n");

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		pr_info(" - stop bits = 2\n");
	else
		pr_info(" - stop bits = 1\n");

	/* figure out the flow control settings */
	if (cflag & CRTSCTS)
		pr_info(" - RTS/CTS is enabled\n");
	else
		pr_info(" - RTS/CTS is disabled\n");

	/* Set baud rate */
        baud = uart_get_baud_rate(port, new, old, 0, port->uartclk/16);
		if(baud == 0)
			pr_warn("get baud rate 0\n");
		else
			quot = uart_get_divisor(port, baud);
	
	//UART_PUT_DIV_LO(port, (quot & 0xff));
	//UART_PUT_DIV_HI(port, ((quot & 0xf00) >> 8));
}

static int tiny_startup(struct uart_port *port)
{
	pr_enter();

	/* this is the first time this port is opened */
	/* do any hardware initialization needed here */

	/* create our timer and submit it */
	if (!timer) {
		timer = kmalloc(sizeof(*timer), GFP_KERNEL);
		if (!timer)
			return -ENOMEM;
	}
	init_timer(timer);
	timer->data = (unsigned long)port;
	timer->expires = jiffies + DELAY_TIME;
	timer->function = tiny_timer;
	add_timer(timer);
	return 0;
}

static void tiny_shutdown(struct uart_port *port)
{
	pr_enter();

	/* The port is being closed by the last user. */
	/* Do any hardware specific stuff here */

	/* shut down our timer */
	del_timer(timer);
}

static const char *tiny_type(struct uart_port *port)
{
	pr_enter();
	return "tinytty";
}

static void tiny_release_port(struct uart_port *port)
{
	pr_enter();
}

static int tiny_request_port(struct uart_port *port)
{
	pr_enter();
	return 0;
}

static void tiny_config_port(struct uart_port *port, int flags)
{
	pr_enter();
}

static int tiny_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	pr_enter();
	return 0;
}

static struct uart_ops tiny_ops = {
	.tx_empty	= tiny_tx_empty,
	.set_mctrl	= tiny_set_mctrl,
	.get_mctrl	= tiny_get_mctrl,
	.stop_tx	= tiny_stop_tx,
	.start_tx	= tiny_start_tx,
	.stop_rx	= tiny_stop_rx,
	.enable_ms	= tiny_enable_ms,
	.break_ctl	= tiny_break_ctl,
	.startup	= tiny_startup,
	.shutdown	= tiny_shutdown,
	.set_termios	= tiny_set_termios,
	.type		= tiny_type,
	.release_port	= tiny_release_port,
	.request_port	= tiny_request_port,
	.config_port	= tiny_config_port,
	.verify_port	= tiny_verify_port,
};

static struct uart_port tiny_port = {
	.uartclk	= TINY_CLOCK,
	.ops		= &tiny_ops,
	.type		= PORT_16550A,
};

static struct uart_driver tiny_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= TINY_SERIAL_NAME,
	.dev_name	= TINY_SERIAL_NAME,
	.major		= TINY_SERIAL_MAJOR,
	.minor		= TINY_SERIAL_MINORS,
	.nr		= UART_NR,
};

static int __init tiny_init(void)
{
	int result;

	printk(KERN_INFO "Tiny serial driver loaded\n");

	result = uart_register_driver(&tiny_reg);
	if (result) {
		pr_warn("uart_register_driver failed.\n");
		return result;
	}

	result = uart_add_one_port(&tiny_reg, &tiny_port);
	if (result) {
		pr_warn("uart_add_one_port failed.\n");
		uart_unregister_driver(&tiny_reg);
	}

	return result;
}

static void __exit tiny_exit(void)
{
	uart_remove_one_port(&tiny_reg, &tiny_port);
	uart_unregister_driver(&tiny_reg);

	if (!timer)
	  kfree(timer);
}

module_init(tiny_init);
module_exit(tiny_exit);
