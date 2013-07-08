/*
    comedi/drivers/ssv_dnp.c
    generic comedi driver for SSV Embedded Systems' DIL/Net-PCs
    Copyright (C) 2001 Robert Schwebel <robert@schwebel.de>

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 2000 David A. Schleef <ds@schleef.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
/*
Driver: ssv_dnp
Description: SSV Embedded Systems DIL/Net-PC
Author: Robert Schwebel <robert@schwebel.de>
Devices: [SSV Embedded Systems] DIL/Net-PC 1486 (dnp-1486)
Status: unknown
*/

/* include files ----------------------------------------------------------- */

#include "../comedidev.h"

/* Some global definitions: the registers of the DNP ----------------------- */
/*                                                                           */
/* For port A and B the mode register has bits corresponding to the output   */
/* pins, where Bit-N = 0 -> input, Bit-N = 1 -> output. Note that bits       */
/* 4 to 7 correspond to pin 0..3 for port C data register. Ensure that bits  */
/* 0..3 remain unchanged! For details about Port C Mode Register see         */
/* the remarks in dnp_insn_config() below.                                   */

#define CSCIR 0x22		/* Chip Setup and Control Index Register     */
#define CSCDR 0x23		/* Chip Setup and Control Data Register      */
#define PAMR  0xa5		/* Port A Mode Register                      */
#define PADR  0xa9		/* Port A Data Register                      */
#define PBMR  0xa4		/* Port B Mode Register                      */
#define PBDR  0xa8		/* Port B Data Register                      */
#define PCMR  0xa3		/* Port C Mode Register                      */
#define PCDR  0xa7		/* Port C Data Register                      */

/* ------------------------------------------------------------------------- */
/* The insn_bits interface allows packed reading/writing of DIO channels.    */
/* The comedi core can convert between insn_bits and insn_read/write, so you */
/* are able to use these instructions as well.                               */
/* ------------------------------------------------------------------------- */

static int dnp_dio_insn_bits(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data)
{
	/* The insn data is a mask in data[0] and the new data in data[1],   */
	/* each channel cooresponding to a bit.                              */

	/* Ports A and B are straight forward: each bit corresponds to an    */
	/* output pin with the same order. Port C is different: bits 0...3   */
	/* correspond to bits 4...7 of the output register (PCDR).           */

	if (data[0]) {

		outb(PADR, CSCIR);
		outb((inb(CSCDR)
		      & ~(u8) (data[0] & 0x0000FF))
		     | (u8) (data[1] & 0x0000FF), CSCDR);

		outb(PBDR, CSCIR);
		outb((inb(CSCDR)
		      & ~(u8) ((data[0] & 0x00FF00) >> 8))
		     | (u8) ((data[1] & 0x00FF00) >> 8), CSCDR);

		outb(PCDR, CSCIR);
		outb((inb(CSCDR)
		      & ~(u8) ((data[0] & 0x0F0000) >> 12))
		     | (u8) ((data[1] & 0x0F0000) >> 12), CSCDR);
	}

	/* on return, data[1] contains the value of the digital input lines. */
	outb(PADR, CSCIR);
	data[0] = inb(CSCDR);
	outb(PBDR, CSCIR);
	data[0] += inb(CSCDR) << 8;
	outb(PCDR, CSCIR);
	data[0] += ((inb(CSCDR) & 0xF0) << 12);

	return insn->n;

}

/* ------------------------------------------------------------------------- */
/* Configure the direction of the bidirectional digital i/o pins. chanspec   */
/* contains the channel to be changed and data[0] contains either            */
/* COMEDI_INPUT or COMEDI_OUTPUT.                                            */
/* ------------------------------------------------------------------------- */

static int dnp_dio_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{

	u8 register_buffer;

	/* reduces chanspec to lower 16 bits */
	int chan = CR_CHAN(insn->chanspec);

	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
	case INSN_CONFIG_DIO_INPUT:
		break;
	case INSN_CONFIG_DIO_QUERY:
		data[1] =
		    (inb(CSCDR) & (1 << chan)) ? COMEDI_OUTPUT : COMEDI_INPUT;
		return insn->n;
		break;
	default:
		return -EINVAL;
		break;
	}
	/* Test: which port does the channel belong to?                       */

	/* We have to pay attention with port C: this is the meaning of PCMR: */
	/* Bit in PCMR:              7 6 5 4 3 2 1 0                          */
	/* Corresponding port C pin: d 3 d 2 d 1 d 0   d= don't touch         */

	if ((chan >= 0) && (chan <= 7)) {
		/* this is port A */
		outb(PAMR, CSCIR);
	} else if ((chan >= 8) && (chan <= 15)) {
		/* this is port B */
		chan -= 8;
		outb(PBMR, CSCIR);
	} else if ((chan >= 16) && (chan <= 19)) {
		/* this is port C; multiplication with 2 brings bits into     */
		/* correct position for PCMR!                                 */
		chan -= 16;
		chan *= 2;
		outb(PCMR, CSCIR);
	} else {
		return -EINVAL;
	}

	/* read 'old' direction of the port and set bits (out=1, in=0)        */
	register_buffer = inb(CSCDR);
	if (data[0] == COMEDI_OUTPUT)
		register_buffer |= (1 << chan);
	else
		register_buffer &= ~(1 << chan);

	outb(register_buffer, CSCDR);

	return 1;

}

static int dnp_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int ret;

	dev->board_name = dev->driver->driver_name;

	ret = comedi_alloc_subdevices(dev, 1);
	if (ret)
		return ret;

	s = &dev->subdevices[0];
	/* digital i/o subdevice                                             */
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 20;
	s->maxdata = 1;
	s->range_table = &range_digital;
	s->insn_bits = dnp_dio_insn_bits;
	s->insn_config = dnp_dio_insn_config;

	/* We use the I/O ports 0x22,0x23 and 0xa3-0xa9, which are always
	 * allocated for the primary 8259, so we don't need to allocate them
	 * ourselves. */

	/* configure all ports as input (default)                            */
	outb(PAMR, CSCIR);
	outb(0x00, CSCDR);
	outb(PBMR, CSCIR);
	outb(0x00, CSCDR);
	outb(PCMR, CSCIR);
	outb((inb(CSCDR) & 0xAA), CSCDR);

	dev_info(dev->class_dev, "%s: attached\n", dev->board_name);
	return 1;
}

static void dnp_detach(struct comedi_device *dev)
{
	outb(PAMR, CSCIR);
	outb(0x00, CSCDR);
	outb(PBMR, CSCIR);
	outb(0x00, CSCDR);
	outb(PCMR, CSCIR);
	outb((inb(CSCDR) & 0xAA), CSCDR);
}

static struct comedi_driver dnp_driver = {
	.driver_name	= "dnp-1486",
	.module		= THIS_MODULE,
	.attach		= dnp_attach,
	.detach		= dnp_detach,
};
module_comedi_driver(dnp_driver);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
