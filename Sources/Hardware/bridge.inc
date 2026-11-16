// SPDX-License-Identifier: GPL-2.0-only
// Userspace adaptation modified by Dylan Ross, 2026-11-15; original notices retained below.
/*
 * ITE IT930x USB bridge driver for CXD28xx demod + tuner
 *
 * Copyright (c) 2026 Yoonji Park <koreapyj@dcmys.kr>
 */



/* Command IDs */
#define IT930X_CMD_REG_READ		0x0000
#define IT930X_CMD_REG_WRITE		0x0001
#define IT930X_CMD_QUERYINFO		0x0022
#define IT930X_CMD_BOOT			0x0023
#define IT930X_CMD_FW_SCATTER_WRITE	0x0029
#define IT930X_CMD_GENERIC_I2C_RD	0x002a
#define IT930X_CMD_GENERIC_I2C_WR	0x002b
#define IT930X_CMD_UART_READ		0x0033
#define IT930X_CMD_UART_WRITE		0x0034
#define IT930X_CMD_UART_CTRL		0x0035

/* USB endpoints */
#define IT930X_EP_CMD_OUT		0x02
#define IT930X_EP_CMD_IN		0x81
#define IT930X_EP_TS			0x84

/* Protocol limits */
#define IT930X_CMD_MAX_PKT		512
#define IT930X_CMD_MAX_PAYLOAD		250
#define IT930X_CMD_HDR_LEN		4
#define IT930X_CMD_CSUM_LEN		2
#define IT930X_USB_TIMEOUT_MS		1000

/* I2C limits */
#define IT930X_I2C_SHORT_MAX		40
#define IT930X_I2C_STAGE_CHUNK		40

/* TS streaming */
#define IT930X_URB_COUNT		8
#define IT930X_URB_BUF_SIZE		(188 * 348)

/* Bridge registers */
#define IT930X_REG_CLK_DISABLE		0x4976
#define IT930X_REG_PLL_RESET		0x4bfb
#define IT930X_REG_TS_CLK_CFG		0x4978
#define IT930X_REG_EXT_CLK_RESET	0x4977
#define IT930X_REG_I2C_BUS0_SPEED	0xf6a7
#define IT930X_REG_I2C_BUS1_SPEED	0xf103
#define IT930X_REG_I2C_TRIG		0x4900
#define IT930X_REG_I2C_READ_BUF	0xf000
#define IT930X_REG_I2C_WRITE_BUF	0xf001
#define IT930X_REG_I2C_BUS_CTRL	0xf424
#define IT930X_REG_TS_FAIL_IGNORE	0xda5a
#define IT930X_REG_TS_SYNC_BYPASS	0xda1a
#define IT930X_REG_TS_OUT_EN		0xd833
#define IT930X_REG_TS_OUT_CFG0		0xd830
#define IT930X_REG_TS_OUT_CFG1		0xd831
#define IT930X_REG_TS_OUT_CFG2		0xd832

#define IT930X_PID_FILTER_MAX		64

/* Smartcard / UART registers */
#define IT930X_SC_UART_READY		0x496A
#define IT930X_SC_UART_RXCOUNT		0x496B
#define IT930X_SC_UART_MODE		0x7904
#define IT930X_SC_UART_ENABLE		0x4965	/* Type 2 TX enable */
#define IT930X_SC_BOARD_TYPE_REG		0x49E8

#define IT930X_SC_GPIO_DETECT		6	/* GPIOH6 - card detect (active low) */
#define IT930X_SC_GPIO_TXEN		3	/* GPIOH3 - TX enable, Type 1 */
#define IT930X_SC_UART_WR_CHUNK		48
#define IT930X_SC_UART_RD_CHUNK		32
#define IT930X_SC_POLL_COUNT		50
#define IT930X_SC_POLL_MS		10

/* -------- GPIO register table -------- */

/*
 * IT930x has 16 GPIOs. Each GPIO has three registers:
 *   en  — direction (1 = output)
 *   on  — enable    (1 = enabled)
 *   val — output value
 * Indexed 0-15 for GPIO 1-16.
 */
struct it930x_gpio_regs {
	u32 en;
	u32 on;
	u32 val;
};

static const struct it930x_gpio_regs it930x_gpio[16] = {
	{ 0xd8b0, 0xd8b1, 0xd8af },	/* GPIO 1  */
	{ 0xd8b8, 0xd8b9, 0xd8b7 },	/* GPIO 2  */
	{ 0xd8b4, 0xd8b5, 0xd8b3 },	/* GPIO 3  */
	{ 0xd8c0, 0xd8c1, 0xd8bf },	/* GPIO 4  */
	{ 0xd8bc, 0xd8bd, 0xd8bb },	/* GPIO 5  */
	{ 0xd8c8, 0xd8c9, 0xd8c7 },	/* GPIO 6  */
	{ 0xd8c4, 0xd8c5, 0xd8c3 },	/* GPIO 7  */
	{ 0xd8d0, 0xd8d1, 0xd8cf },	/* GPIO 8  */
	{ 0xd8cc, 0xd8cd, 0xd8cb },	/* GPIO 9  */
	{ 0xd8d8, 0xd8d9, 0xd8d7 },	/* GPIO 10 */
	{ 0xd8d4, 0xd8d5, 0xd8d3 },	/* GPIO 11 */
	{ 0xd8e0, 0xd8e1, 0xd8df },	/* GPIO 12 */
	{ 0xd8dc, 0xd8dd, 0xd8db },	/* GPIO 13 */
	{ 0xd8e4, 0xd8e5, 0xd8e3 },	/* GPIO 14 */
	{ 0xd8e8, 0xd8e9, 0xd8e7 },	/* GPIO 15 */
	{ 0xd8ec, 0xd8ed, 0xd8eb },	/* GPIO 16 */
};

/* -------- Board configuration -------- */

#define IT930X_MAX_FRONTENDS	5

struct it930x_fe_cfg {
	u8 ts_port;	/* IT930x stream input port (0-4) */
	u8 i2c_bus;	/* IT930x internal bus: 1 or 3 */
	u8 demod_addr;	/* demod SLVT 7-bit I2C address */
	u8 tuner_addr;	/* tuner 7-bit I2C address */
};

struct it930x_board_cfg {
	const char		*name;
	const char		*fw_file;
	int			num_frontends;
	struct it930x_fe_cfg	fe[IT930X_MAX_FRONTENDS];
	u8			gpio_power;	/* backend power GPIO (active low), 0=none */
	u8			gpio_reset;	/* demod reset GPIO (pulse low) */
	u8			gpio_lnb;	/* LNB power GPIO (active high), 0=none */
	u8			gpio_always_hi;	/* GPIO set high at init, 0=none */
	u8			i2c_notify;	/* override 0x4975 value, 0=use demod_addr<<1 */
	enum sony_ascot3_xtal_t	tuner_xtal;
	u8			gpio_sc_reset;	/* smartcard reset GPIO, 0=no smartcard */
};
static u16 it930x_checksum(const u8 *buf, size_t len)
{
	u32 sum = 0;
	size_t i;

	for (i = 0; i + 1 < len; i += 2)
		sum += ((u16)buf[i] << 8) | buf[i + 1];

	if (len & 1)
		sum += (u16)buf[len - 1] << 8;

	return (u16)~sum;
}

/* -------- Command submission (synchronous bulk) -------- */

static int it930x_cmd_submit_sync(struct it930x_dev *dev, u16 cmd,
				  const u8 *tx_payload, u8 tx_payload_len,
				  u8 *rx_payload, u8 rx_payload_len)
{
	u8 *tx = dev->tx_bulk_buf;
	u8 *rx = dev->rx_bulk_buf;
	int tx_len, rx_len, actual;
	u16 csum, rx_csum;
	int ret;

	if (tx_payload_len > IT930X_CMD_MAX_PAYLOAD ||
	    rx_payload_len > IT930X_CMD_MAX_PAYLOAD)
		return -E2BIG;

	mutex_lock(&dev->cmd_mutex);

	/* Build TX frame */
	tx_len = IT930X_CMD_HDR_LEN + tx_payload_len + IT930X_CMD_CSUM_LEN;

	tx[1] = cmd >> 8;
	tx[2] = cmd & 0xff;
	tx[3] = dev->seq++;
	if (tx_payload_len && tx_payload)
		memcpy(&tx[4], tx_payload, tx_payload_len);

	csum = it930x_checksum(&tx[1], 3 + tx_payload_len);
	tx[4 + tx_payload_len] = csum >> 8;
	tx[5 + tx_payload_len] = csum & 0xff;
	tx[0] = tx_len - 1;

	/* Send command */
	ret = usb_bulk_msg(dev->udev,
			   usb_sndbulkpipe(dev->udev, IT930X_EP_CMD_OUT),
			   tx, tx_len, &actual, IT930X_USB_TIMEOUT_MS);
	if (ret)
		goto out;

	/* Receive response */
	ret = usb_bulk_msg(dev->udev,
			   usb_rcvbulkpipe(dev->udev, IT930X_EP_CMD_IN),
			   rx, IT930X_CMD_MAX_PKT, &rx_len,
			   IT930X_USB_TIMEOUT_MS);
	if (ret)
		goto out;

	if (rx_len < 5) {
		ret = -EPROTO;
		goto out;
	}

	/* Validate checksum */
	rx_csum = ((u16)rx[rx_len - 2] << 8) | rx[rx_len - 1];
	csum = it930x_checksum(&rx[1], rx_len - 3);
	if (csum != rx_csum) {
		ret = -EPROTO;
		goto out;
	}

	/* Check result byte */
	if (rx[2]) {
		dev_dbg(&dev->intf->dev,
			"cmd 0x%04x failed: result=%u\n", cmd, rx[2]);
		ret = -EIO;
		goto out;
	}

	/* Copy response payload */
	if (rx_payload_len && rx_payload) {
		if (rx_len - 5 < rx_payload_len) {
			ret = -EPROTO;
			goto out;
		}
		memcpy(rx_payload, &rx[3], rx_payload_len);
	}

out:
	mutex_unlock(&dev->cmd_mutex);
	return ret;
}

/* -------- Bridge register access -------- */

static int it930x_write_regs(struct it930x_dev *dev, u32 reg, const u8 *buf,
			     u8 len)
{
	u8 payload[6 + 250];

	if (len > 250)
		return -E2BIG;

	payload[0] = len;
	payload[1] = 2; /* processor (OFDM) */
	payload[2] = (reg >> 24) & 0xff;
	payload[3] = (reg >> 16) & 0xff;
	payload[4] = (reg >> 8) & 0xff;
	payload[5] = reg & 0xff;
	if (len)
		memcpy(&payload[6], buf, len);

	return it930x_cmd_submit_sync(dev, IT930X_CMD_REG_WRITE,
				      payload, 6 + len, NULL, 0);
}

static int it930x_read_regs(struct it930x_dev *dev, u32 reg, u8 *buf, u8 len)
{
	u8 payload[6];

	if (len > 250)
		return -E2BIG;

	payload[0] = len;
	payload[1] = 2; /* processor (OFDM) */
	payload[2] = (reg >> 24) & 0xff;
	payload[3] = (reg >> 16) & 0xff;
	payload[4] = (reg >> 8) & 0xff;
	payload[5] = reg & 0xff;

	return it930x_cmd_submit_sync(dev, IT930X_CMD_REG_READ,
				      payload, sizeof(payload), buf, len);
}

static int it930x_write_reg(struct it930x_dev *dev, u32 reg, u8 val)
{
	return it930x_write_regs(dev, reg, &val, 1);
}

static int it930x_read_reg(struct it930x_dev *dev, u32 reg, u8 *val)
{
	return it930x_read_regs(dev, reg, val, 1);
}

static int it930x_patch_reg_bit(struct it930x_dev *dev, u32 reg, u8 val,
				u8 mask)
{
	u8 cur, next;
	int ret;

	ret = it930x_read_reg(dev, reg, &cur);
	if (ret)
		return ret;

	next = (cur & ~mask) | (val & mask);
	if (next == cur)
		return 0;

	return it930x_write_reg(dev, reg, next);
}

/* -------- GPIO helpers -------- */

/*
 * Set a GPIO to output mode with the given value.
 * Caller must hold dev->io_lock.
 */
static int it930x_gpio_set(struct it930x_dev *dev, u8 gpio, bool val)
{
	const struct it930x_gpio_regs *g;
	int ret;

	if (gpio < 1 || gpio > 16)
		return -EINVAL;

	g = &it930x_gpio[gpio - 1];

	ret = it930x_write_reg(dev, g->en, 0x01);	/* direction = output */
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, g->on, 0x01);	/* enable */
	if (ret)
		return ret;
	return it930x_write_reg(dev, g->val, val ? 0x01 : 0x00);
}

/*
 * Read a GPIO input value.
 * Input register is at val_reg - 1 (separate from output register).
 * Caller must hold dev->io_lock.
 */
static int it930x_gpio_read(struct it930x_dev *dev, u8 gpio, bool *val)
{
	const struct it930x_gpio_regs *g;
	u8 v;
	int ret;

	if (gpio < 1 || gpio > 16)
		return -EINVAL;

	g = &it930x_gpio[gpio - 1];

	ret = it930x_write_reg(dev, g->on, 0x01);	/* enable */
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, g->en, 0x00);	/* direction = input */
	if (ret)
		return ret;
	ret = it930x_read_reg(dev, g->val - 1, &v);	/* input register */
	if (ret)
		return ret;

	*val = !!v;
	return 0;
}

/*
 * Pulse a GPIO low then high (reset pulse).
 * Caller must hold dev->io_lock.
 */
static int it930x_gpio_pulse(struct it930x_dev *dev, u8 gpio)
{
	const struct it930x_gpio_regs *g;
	int ret;

	if (gpio < 1 || gpio > 16)
		return -EINVAL;

	g = &it930x_gpio[gpio - 1];

	ret = it930x_write_reg(dev, g->val, 0x01);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, g->en, 0x01);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, g->on, 0x01);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, g->val, 0x00);
	if (ret)
		return ret;
	return it930x_write_reg(dev, g->val, 0x01);
}

/* -------- Firmware loading -------- */

static int it930x_read_firmware_version(struct it930x_dev *dev, u32 *ver)
{
	u8 tx = 0x01;
	u8 rx[4];
	int ret;

	ret = it930x_cmd_submit_sync(dev, IT930X_CMD_QUERYINFO,
				     &tx, 1, rx, sizeof(rx));
	if (ret)
		return ret;

	if (ver)
		*ver = ((u32)rx[0] << 24) | ((u32)rx[1] << 16) |
		       ((u32)rx[2] << 8) | rx[3];
	return 0;
}

static int it930x_load_firmware(struct it930x_dev *dev)
{
	const struct firmware *fw;
	const char *fw_file = dev->board->fw_file;
	size_t i = 0;
	int ret;

	ret = request_firmware(&fw, fw_file, &dev->intf->dev);
	if (ret) {
		dev_err(&dev->intf->dev,
			"failed to load firmware %s (%d)\n", fw_file, ret);
		return ret;
	}

	while (i < fw->size) {
		const u8 *p = &fw->data[i];
		size_t block_len = 0;
		size_t payload_len;
		u8 seg_cnt;
		unsigned int j;

		if (i + 7 > fw->size) {
			ret = -ECANCELED;
			dev_err(&dev->intf->dev,
				"truncated firmware block at %zu\n", i);
			goto out;
		}

		if (p[0] != 0x03) {
			ret = -ECANCELED;
			dev_err(&dev->intf->dev,
				"bad firmware block marker 0x%02x at %zu\n",
				p[0], i);
			goto out;
		}

		seg_cnt = p[3];
		payload_len = 4 + ((size_t)seg_cnt * 3);
		if (i + payload_len > fw->size) {
			ret = -ECANCELED;
			dev_err(&dev->intf->dev,
				"truncated firmware segment table at %zu\n", i);
			goto out;
		}

		for (j = 0; j < seg_cnt; j++)
			block_len += p[6 + (j * 3)];

		if (!block_len) {
			i += payload_len;
			continue;
		}

		payload_len += block_len;
		if (i + payload_len > fw->size) {
			ret = -ECANCELED;
			dev_err(&dev->intf->dev,
				"truncated firmware data at %zu\n", i);
			goto out;
		}
		if (payload_len > IT930X_CMD_MAX_PAYLOAD) {
			ret = -E2BIG;
			dev_err(&dev->intf->dev,
				"firmware block too large (%zu) at %zu\n",
				payload_len, i);
			goto out;
		}

		ret = it930x_cmd_submit_sync(dev, IT930X_CMD_FW_SCATTER_WRITE,
					     p, (u8)payload_len, NULL, 0);
		if (ret) {
			dev_err(&dev->intf->dev,
				"firmware scatter write failed at %zu (%d)\n",
				i, ret);
			goto out;
		}

		i += payload_len;
	}

	ret = it930x_cmd_submit_sync(dev, IT930X_CMD_BOOT, NULL, 0, NULL, 0);
	if (ret)
		dev_err(&dev->intf->dev, "firmware boot failed (%d)\n", ret);

out:
	release_firmware(fw);
	return ret;
}

/* -------- I2C relay -------- */

static int it930x_i2c_generic_write(struct it930x_dev *dev, u8 bus,
				    u8 slave, const u8 *buf, u8 len)
{
	u8 payload[3 + IT930X_I2C_SHORT_MAX];

	if (len > IT930X_I2C_SHORT_MAX)
		return -E2BIG;

	payload[0] = len;
	payload[1] = bus;
	payload[2] = slave;
	if (len)
		memcpy(&payload[3], buf, len);

	return it930x_cmd_submit_sync(dev, IT930X_CMD_GENERIC_I2C_WR,
				      payload, 3 + len, NULL, 0);
}

static int it930x_i2c_generic_read(struct it930x_dev *dev, u8 bus,
				   u8 slave, u8 *buf, u8 len)
{
	u8 payload[3];

	if (len > 250)
		return -E2BIG;

	payload[0] = len;
	payload[1] = bus;
	payload[2] = slave;

	return it930x_cmd_submit_sync(dev, IT930X_CMD_GENERIC_I2C_RD,
				      payload, sizeof(payload), buf, len);
}

static int it930x_i2c_write(struct it930x_dev *dev, u8 bus, u8 slave,
			    const u8 *buf, u8 len)
{
	u8 trig[4];
	u8 chunk;
	int ret;
	u32 off = 0;
	u32 remaining = len;

	if (!len)
		return 0;

	if (len <= IT930X_I2C_SHORT_MAX)
		return it930x_i2c_generic_write(dev, bus, slave, buf, len);

	if (len > U8_MAX)
		return -E2BIG;

	/* Stage long write into bridge buffer */
	while (remaining) {
		chunk = min_t(u32, remaining, IT930X_I2C_STAGE_CHUNK);
		ret = it930x_write_regs(dev, IT930X_REG_I2C_WRITE_BUF + off,
					&buf[off], chunk);
		if (ret)
			return ret;

		off += chunk;
		remaining -= chunk;
	}

	/* Trigger I2C write */
	trig[0] = 0xf4;
	trig[1] = bus;
	trig[2] = slave;
	trig[3] = len;

	return it930x_write_regs(dev, IT930X_REG_I2C_TRIG, trig, sizeof(trig));
}

static int it930x_i2c_read(struct it930x_dev *dev, u8 bus, u8 slave,
			   u8 *buf, u8 len)
{
	u8 trig[4];
	int ret;

	if (!len)
		return 0;

	if (len <= IT930X_I2C_SHORT_MAX)
		return it930x_i2c_generic_read(dev, bus, slave, buf, len);

	if (len > U8_MAX)
		return -E2BIG;

	/* Trigger I2C read */
	trig[0] = 0xf5;
	trig[1] = bus;
	trig[2] = slave;
	trig[3] = len;
	ret = it930x_write_regs(dev, IT930X_REG_I2C_TRIG, trig, sizeof(trig));
	if (ret)
		return ret;

	return it930x_read_regs(dev, IT930X_REG_I2C_READ_BUF, buf, len);
}

/* -------- I2C adapter -------- */

/*
 * I2C repeated-start via F424 bus control.
 *
 * The IT930x bridge uses register F424 to implement I2C repeated-start:
 *   F424 = 0x01: hold bus (suppress STOP after next I2C write)
 *   F424 = 0x00: release bus (normal STOP behavior)
 */
static int it930x_i2c_xfer_rstart(struct it930x_dev *dev, u8 bus,
				  struct i2c_msg *wr, struct i2c_msg *rd)
{
	u8 wr_slave = wr->addr << 1;
	u8 rd_slave = rd->addr << 1;
	int ret;

	ret = it930x_write_reg(dev, IT930X_REG_I2C_BUS_CTRL, 0x01);
	if (ret)
		return ret;

	ret = it930x_i2c_write(dev, bus, wr_slave, wr->buf, wr->len);
	if (ret) {
		it930x_write_reg(dev, IT930X_REG_I2C_BUS_CTRL, 0x00);
		return ret;
	}

	ret = it930x_write_reg(dev, IT930X_REG_I2C_BUS_CTRL, 0x00);
	if (ret)
		return ret;

	return it930x_i2c_read(dev, bus, rd_slave, rd->buf, rd->len);
}

static int it930x_i2c_xfer(struct i2c_adapter *adap,
			    struct i2c_msg *msgs, int num)
{
	struct it930x_i2c_ctx *ctx = i2c_get_adapdata(adap);
	struct it930x_dev *dev = ctx->dev;
	u8 bus = ctx->bus_num;
	int i, ret = 0;

	mutex_lock(&dev->io_lock);

	if (num == 2 &&
	    !(msgs[0].flags & I2C_M_RD) && (msgs[1].flags & I2C_M_RD) &&
	    msgs[0].len <= U8_MAX && msgs[1].len <= U8_MAX) {
		ret = it930x_i2c_xfer_rstart(dev, bus, &msgs[0], &msgs[1]);
		if (ret)
			dev_dbg(&dev->intf->dev,
				"i2c rstart addr=0x%02x wr=%u rd=%u failed: %d\n",
				msgs[0].addr << 1, msgs[0].len,
				msgs[1].len, ret);
		mutex_unlock(&dev->io_lock);
		return ret ? ret : num;
	}

	for (i = 0; i < num; i++) {
		u8 slave = msgs[i].addr << 1;

		if (msgs[i].len > U8_MAX) {
			ret = -E2BIG;
			break;
		}

		if (msgs[i].flags & I2C_M_RD)
			ret = it930x_i2c_read(dev, bus,
					      slave, msgs[i].buf, msgs[i].len);
		else
			ret = it930x_i2c_write(dev, bus,
					       slave, msgs[i].buf,
					       msgs[i].len);
		if (ret) {
			dev_dbg(&dev->intf->dev,
				"i2c xfer %s addr=0x%02x len=%u failed: %d\n",
				(msgs[i].flags & I2C_M_RD) ? "rd" : "wr",
				slave, msgs[i].len, ret);
			break;
		}
	}

	mutex_unlock(&dev->io_lock);

	return ret ? ret : num;
}

static int it930x_stage_pre_init(struct it930x_dev *dev)
{
	const struct it930x_board_cfg *board = dev->board;
	int ret;

	ret = it930x_write_reg(dev, 0xda05, 0x01);
	if (ret)
		return ret;

	/* GPIO reset pulse */
	ret = it930x_gpio_pulse(dev, board->gpio_reset);
	if (ret)
		return ret;

	/* GPIO always-high (23e2:2b02: GPIO14 for power rail) */
	if (board->gpio_always_hi) {
		ret = it930x_gpio_set(dev, board->gpio_always_hi, true);
		if (ret)
			return ret;
	}

	/* Backend power GPIO: set HIGH initially (power off, active low) */
	if (board->gpio_power) {
		ret = it930x_gpio_set(dev, board->gpio_power, true);
		if (ret)
			return ret;
	}

	/* LNB power GPIO: set LOW initially (off) */
	if (board->gpio_lnb) {
		ret = it930x_gpio_set(dev, board->gpio_lnb, false);
		if (ret)
			return ret;
	}

	/* Clock/PLL/I2C speed common init */
	ret = it930x_write_reg(dev, IT930X_REG_CLK_DISABLE, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_PLL_RESET, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_TS_CLK_CFG, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_EXT_CLK_RESET, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_I2C_BUS0_SPEED, 0x07);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_I2C_BUS1_SPEED, 0x07);
	if (ret)
		return ret;
	return it930x_write_reg(dev, IT930X_REG_TS_SYNC_BYPASS, 0x00);
}

static int it930x_stage_fw_ready(struct it930x_dev *dev)
{
	u32 fw_version = 0;
	int ret;

	ret = it930x_read_firmware_version(dev, &fw_version);
	if (!ret && fw_version) {
		dev->fw_version = fw_version;
		dev_info(&dev->intf->dev, "firmware version: %u.%u.%u.%u\n",
			 (fw_version >> 24) & 0xff,
			 (fw_version >> 16) & 0xff,
			 (fw_version >> 8) & 0xff,
			 fw_version & 0xff);
		return 0;
	}

	dev_info(&dev->intf->dev, "cold boot, loading firmware\n");

	ret = it930x_load_firmware(dev);
	if (ret)
		return ret;

	ret = it930x_read_firmware_version(dev, &fw_version);
	if (ret)
		return ret;
	if (!fw_version)
		return -ENODEV;

	dev->fw_version = fw_version;
	dev_info(&dev->intf->dev, "firmware version: %u.%u.%u.%u\n",
		 (fw_version >> 24) & 0xff, (fw_version >> 16) & 0xff,
		 (fw_version >> 8) & 0xff, fw_version & 0xff);

	return 0;
}

/*
 * Post-boot bridge configuration.
 * Key operations: I2C controller config, TS clock toggle, I2C slave
 * address registration, GPIO reset pulse, TS output config.
 */
static int it930x_stage_post_boot(struct it930x_dev *dev)
{
	const struct it930x_board_cfg *board = dev->board;
	static const u32 addr_regs[] = { 0x4975, 0x4974, 0x4973, 0x4972, 0x4964 };
	static const u32 bus_regs[]  = { 0x4971, 0x4970, 0x496f, 0x496e, 0x4963 };
	u8 dd88[2] = { IT930X_URB_BUF_SIZE / 4 & 0xff,
		       (IT930X_URB_BUF_SIZE / 4 >> 8) & 0xff };
	u8 val;
	int ret, i;

	/* I2C bus speeds (must re-set after firmware boot) */
	ret = it930x_write_reg(dev, IT930X_REG_I2C_BUS0_SPEED, 0x07);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_I2C_BUS1_SPEED, 0x07);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_TS_SYNC_BYPASS, 0x00);
	if (ret)
		return ret;

	/* TS output pin patch — set bit 2 of f41f */
	ret = it930x_patch_reg_bit(dev, 0xf41f, 0x04, 0x04);
	if (ret)
		return ret;

	/* Bridge I2C/TS config registers */
	ret = it930x_write_reg(dev, 0xda10, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xf41a, 0x01);
	if (ret)
		return ret;

	/* TS port enable — first pass (set bit 0) */
	ret = it930x_write_reg(dev, 0xda1d, 0x01);
	if (ret)
		return ret;

	/*
	 * TS clock config with bit 5 toggle on dd11.
	 * This toggle is required even on warm boot.
	 */
	ret = it930x_read_reg(dev, 0xdd11, &val);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xdd11, val & ~0x20);
	if (ret)
		return ret;

	ret = it930x_write_reg(dev, 0xdd13, 0x00);
	if (ret)
		return ret;

	ret = it930x_read_reg(dev, 0xdd11, &val);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xdd11, val | 0x20);
	if (ret)
		return ret;

	/* DD88 / DD0C */
	ret = it930x_write_regs(dev, 0xdd88, dd88, sizeof(dd88));
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xdd0c, 0x80);
	if (ret)
		return ret;

	/* Clock save/restore registers */
	ret = it930x_write_reg(dev, 0xda05, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xda06, 0x00);
	if (ret)
		return ret;

	/* TS port disable — second pass (clear bit 0) */
	ret = it930x_write_reg(dev, 0xda1d, 0x00);
	if (ret)
		return ret;

	/* TS output config */
	ret = it930x_write_reg(dev, 0xd920, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_TS_OUT_EN, 0x01);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_TS_OUT_CFG0, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_TS_OUT_CFG1, 0x01);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, IT930X_REG_TS_OUT_CFG2, 0x00);
	if (ret)
		return ret;

	/* I2C slave address/bus registration for each frontend */
	ret = it930x_write_reg(dev, IT930X_REG_CLK_DISABLE, 0x01);
	if (ret)
		return ret;

	for (i = 0; i < board->num_frontends; i++) {
		const struct it930x_fe_cfg *fc = &board->fe[i];
		u8 addr_val;

		if (i == 0 && board->i2c_notify)
			addr_val = board->i2c_notify;
		else
			addr_val = fc->demod_addr << 1;

		ret = it930x_write_reg(dev, addr_regs[i], addr_val);
		if (ret)
			return ret;
		ret = it930x_write_reg(dev, bus_regs[i], fc->i2c_bus);
		if (ret)
			return ret;
	}

	/* GPIO11 output high (23e2:2b02 I2C bus enable / PX-MLT LNB off) */
	ret = it930x_gpio_set(dev, 11, true);
	if (ret)
		return ret;

	/* GPIO reset pulse after I2C slave config */
	ret = it930x_gpio_pulse(dev, board->gpio_reset);
	if (ret)
		return ret;

	/* Ignore TS input failures on all ports */
	return it930x_write_reg(dev, IT930X_REG_TS_FAIL_IGNORE, 0x1f);
}

/*
 * TS input port configuration.
 * For single-frontend boards: standard 0x47 sync, no aggregation.
 * For multi-frontend boards: per-port sync byte aggregation.
 */
static int it930x_stage_ts_port_cfg(struct it930x_dev *dev)
{
	const struct it930x_board_cfg *board = dev->board;
	int i, ret;

	/* TS tag */
	ret = it930x_write_reg(dev, 0xda34, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xda51, 0xbc);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xda5f, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xda60, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xda61, 0x00);
	if (ret)
		return ret;
	ret = it930x_write_reg(dev, 0xda62, 0x00);
	if (ret)
		return ret;

	for (i = 0; i < board->num_frontends; i++) {
		u8 port = board->fe[i].ts_port;

		/* Serial mode for ports 0-1 */
		if (port < 2) {
			ret = it930x_write_reg(dev, 0xda58 + port, 0x00);
			if (ret)
				return ret;
		}

		if (board->num_frontends > 1) {
			u8 sync = (u8)(((i + 1) << 4) | 0x07);

			/* Sync-byte aggregation mode */
			ret = it930x_write_reg(dev, 0xda73 + port, 0x01);
			if (ret)
				return ret;
			ret = it930x_write_reg(dev, 0xda78 + port, sync);
			if (ret)
				return ret;
		} else {
			/* Sync-byte aggregation mode */
			ret = it930x_write_reg(dev, 0xda73 + port, 0x01);
			if (ret)
				return ret;
			ret = it930x_write_reg(dev, 0xda78 + port, 0x47);
			if (ret)
				return ret;
		}

		/* Enable port */
		ret = it930x_write_reg(dev, 0xda4c + port, 0x01);
		if (ret)
			return ret;
	}

	return it930x_write_reg(dev, IT930X_REG_TS_FAIL_IGNORE, 0x1f);
}

static int it930x_hw_init(struct it930x_dev *dev)
{
	int ret;

	ret = it930x_stage_pre_init(dev);
	if (ret)
		return ret;

	ret = it930x_stage_fw_ready(dev);
	if (ret)
		return ret;

	ret = it930x_stage_post_boot(dev);
	if (ret)
		return ret;

	dev_info(&dev->intf->dev, "bridge initialization complete\n");
	return 0;
}

/* -------- Endpoint validation -------- */
