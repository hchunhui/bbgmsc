#define MOUSE_ENABLE

#define EM_CMD_SET_REMOTE_MODE	0xF0
#define EM_CMD_READ_DATA		0xEB
#define EM_CMD_RESPONSE			0xFA

enum EM_STATE
{
	EMS_IDLE = 0,
	EMS_RX_COMMAND,
	EMS_TX_DATA
};

// in ch9350_input.c
/*extern*/ unsigned char mouse_key;
/*extern*/ signed short  mouse_delta_x;
/*extern*/ signed short  mouse_delta_y;

static int				bOut;
static unsigned char	ScanNo;
static int				bMouseActived;
static unsigned char	nPrev$4016;
static int				nRead$4016Count;
static int				nRead$4017Count;
static int				nEM84502_State;
static int				nEM84502_Command;
static unsigned char	nEM84502_BitCount;
static int				nEM84502_TxData;
static unsigned char	nEM84502_TxCount;
static unsigned char	nEM84502_Data[3];
static int				bJoyLatch;

static unsigned char	nJoy1Status;
static unsigned char	nJoy2Status;

// joy stick
/*extern*/ int joy1_status;
/*extern*/ int joy2_status;
///*extern*/ unsigned char key_bitmap[32];

extern unsigned char *key_state;
static int MouseGenTxData(unsigned char data);

//#define KEY_SCODE(c) (key_bitmap[(c) / 8] & 1 << ((c) % 8))
#define KEY_SCODE(c) (key_state[(c)])

void Joy_Reset()
{
	bOut = 0;
	ScanNo = 0;
	bJoyLatch = 0;

	bMouseActived = 0;

	nPrev$4016        = 0;
	nRead$4016Count   = 0;
	nRead$4017Count   = 0;

	nEM84502_State    = 0;
	nEM84502_Command  = 0;
	nEM84502_BitCount = 0;
	nEM84502_TxData   = 0;
	nEM84502_TxCount  = 0;
	nEM84502_Data[0]  = 0;
	nEM84502_Data[1]  = 0;
	nEM84502_Data[2]  = 0;
}

void Joy_Write4016(unsigned char data)
{
	nRead$4016Count = 0;

	// mouse
	if (bMouseActived) {
		switch (nEM84502_State) {
			case EMS_IDLE:
				if (data == 0x05) {
					nRead$4017Count = 0;
					nEM84502_Command = 0;
					nEM84502_BitCount = 0;
					nEM84502_State = EMS_RX_COMMAND;
				}
				break;
			case EMS_RX_COMMAND:
				if (data & 4) {
					nEM84502_Command |= (data & 1) ? 0 : (1 << nEM84502_BitCount);
					nEM84502_BitCount++;

					if (22 == nEM84502_BitCount) {
						// command end
						unsigned char cmd;

						cmd = nEM84502_Command & 0xFF;

						nEM84502_TxData = MouseGenTxData(EM_CMD_RESPONSE);

						if (EM_CMD_SET_REMOTE_MODE == cmd) {
							nEM84502_TxCount = 1;
						} else if (EM_CMD_READ_DATA == cmd) {
							unsigned char flag;
							int dx, dy;

							// rev X dir
							dx = -mouse_delta_x;
							dy =  mouse_delta_y;

							mouse_delta_x = 0;
							mouse_delta_y = 0;

							flag = 0;
							if (dx < 0) flag |= 0x20;
							if (dy < 0) flag |= 0x10;

							if (dx > 255 || dx < -256) {
								flag |= 0x80;
								dx = 0;
							}

							if (dy > 255 || dy < -256) {
								flag |= 0x40;
								dy = 0;
							}

							// Key: MRL -> LMR
							nEM84502_Data[0] = ((mouse_key >> 1) | ((mouse_key & 1) << 2)) | flag;
							nEM84502_Data[1] = dy;
							nEM84502_Data[2] = dx;
							nEM84502_TxCount = 4;
						} else {
//							assert(0);
						}

						nRead$4017Count = 0;
						nEM84502_BitCount = 0;
						nEM84502_State = EMS_TX_DATA;
					}
				}
				break;
			case EMS_TX_DATA:
				break;
		}
	} else {
		if (data & 1) {
			bJoyLatch = 1;
		} else if (bJoyLatch) {
			nJoy1Status = joy1_status;
			nJoy2Status = joy2_status;
			bJoyLatch = 0;
		}
	}

	if (data & 4) {
		// keyboard pooling
		if (data == 0x05) {
			bOut = 0;
			ScanNo = 0;
		} else if (data == 0x04) {
			if (++ScanNo > 13)
				ScanNo = 0;
			bOut = !bOut;
		} else if (data == 0x06) {
			bOut = !bOut;
		}
	}
}

unsigned char Joy_Read4016(void)
{
	unsigned char data = 0;

#ifdef MOUSE_ENABLE
	if (!bMouseActived) {
		nRead$4016Count++;
		if (9 == nRead$4016Count) {
			// start mouse reading
			bMouseActived = 1;
			nEM84502_State = EMS_IDLE;
		}
	}
#endif

	// joy stick 1
	data = nJoy1Status & 1;
	nJoy1Status >>= 1;

	return data;
}

// generate raw wave format
static int MouseGenTxData(unsigned char data)
{
	int i;
	int txd;
	int mask;
	int parity;

	// start 4 bits
	txd = 0x0B; // 4'b1011

	// 3'b10d x 8
	mask = 0x10;
	parity = 0;
	for (i = 0; i < 8; i++) {
		if (0 == (data & 1)) {
			txd |= mask;
			parity ^= 1;
		}

		txd |= mask << 2;
		mask <<= 3;
		data >>= 1;
	}

	// 3'b10p
	if (parity) txd |= mask;
	mask <<= 2;
	txd |= mask;

	// 1'b0

// final format:
// 010p10d10d10d10d10d10d10d10d1011

	return txd;
}

unsigned char Joy_Read4017(void)
{
	unsigned char data = 0xFE; // KBD
//	unsigned char data = 0; // JOY Only(for mario)

	nRead$4016Count = 0;

	if (bMouseActived) {
		// mouse
		switch (nEM84502_State) {
			case EMS_IDLE:
				data |= 0x01;
				break;
			case EMS_RX_COMMAND:
				if (0 == (nRead$4017Count & 1))
					data |= 0x01;
				nRead$4017Count++;
				break;
			case EMS_TX_DATA:
				if (nRead$4017Count < 32) {
					if (nEM84502_TxData & (1 << nRead$4017Count))
						data |= 0x01;
					nRead$4017Count++;
				} else {
					nRead$4017Count = 0;
					nEM84502_TxCount--;
					if (nEM84502_TxCount > 0) {
						// load next byte
						unsigned char byte;

						// 3 2 1
						byte = nEM84502_Data[3 - nEM84502_TxCount];

						nEM84502_TxData = MouseGenTxData(byte);
					} else {
						bMouseActived = 0;
						nEM84502_State = EMS_IDLE;
					}
				}
				break;
		}
	} else {
		// joy stick 2
		data |= nJoy2Status & 1;
		nJoy2Status >>= 1;
	}

	switch (ScanNo) {
		case 1:
			if (bOut) {
				if (KEY_SCODE(0x21)) data &= ~0x02; // 4
				if (KEY_SCODE(0x0A)) data &= ~0x04; // G
				if (KEY_SCODE(0x09)) data &= ~0x08; // F
				if (KEY_SCODE(0x06)) data &= ~0x10; // C
			} else {
				if (KEY_SCODE(0x3B)) data &= ~0x02; // F2
				if (KEY_SCODE(0x08)) data &= ~0x04; // E
				if (KEY_SCODE(0x22)) data &= ~0x08; // 5
				if (KEY_SCODE(0x19)) data &= ~0x10; // V
			}
			break;
		case 2:
			if (bOut) {
				if (KEY_SCODE(0x1F)) data &= ~0x02; // 2
				if (KEY_SCODE(0x07)) data &= ~0x04; // D
				if (KEY_SCODE(0x16)) data &= ~0x08; // S
				if (KEY_SCODE(0x4D)) data &= ~0x10; // END
			} else {
				if (KEY_SCODE(0x3A)) data &= ~0x02; // F1
				if (KEY_SCODE(0x1A)) data &= ~0x04; // W
				if (KEY_SCODE(0x20)) data &= ~0x08; // 3
				if (KEY_SCODE(0x1B)) data &= ~0x10; // X
			}
			break;
		case 3:
			if (bOut) {
				if (KEY_SCODE(0x49)) data &= ~0x02; // INSERT
				if (KEY_SCODE(0x2A)) data &= ~0x04; // BACK
				if (KEY_SCODE(0x4E)) data &= ~0x08; // NEXT
				if (KEY_SCODE(0x4F)) data &= ~0x10; // RIGHT
			} else {
				if (KEY_SCODE(0x41)) data &= ~0x02; // F8
				if (KEY_SCODE(0x4B)) data &= ~0x04; // PRIOR
				if (KEY_SCODE(0x4C)) data &= ~0x08; // DELETE
				if (KEY_SCODE(0x4A)) data &= ~0x10; // HOME
			}
			break;
		case 4:
			if (bOut) {
				if (KEY_SCODE(0x26)) data &= ~0x02; // 9
				if (KEY_SCODE(0x0C)) data &= ~0x04; // I
				if (KEY_SCODE(0x0F)) data &= ~0x08; // L
				if (KEY_SCODE(0x36)) data &= ~0x10; // COMMA
			} else {
				if (KEY_SCODE(0x3E)) data &= ~0x02; // F5
				if (KEY_SCODE(0x12)) data &= ~0x04; // O
				if (KEY_SCODE(0x27)) data &= ~0x08; // 0
				if (KEY_SCODE(0x37)) data &= ~0x10; // PERIOD
			}
			break;
		case 5:
			if (bOut) {
				if (KEY_SCODE(0x30)) data &= ~0x02; // RBRACKET
				if (KEY_SCODE(0x28) ||
					KEY_SCODE(0x58)) data &= ~0x04; // RETURN
				if (KEY_SCODE(0x52)) data &= ~0x08; // UP
				if (KEY_SCODE(0x50)) data &= ~0x10; // LEFT
			} else {
				if (KEY_SCODE(0x40)) data &= ~0x02; // F7
				if (KEY_SCODE(0x2F)) data &= ~0x04; // LBRACKET
				if (KEY_SCODE(0x31) ||
					KEY_SCODE(0x32)) data &= ~0x08; // BACKSLASH
				if (KEY_SCODE(0x51)) data &= ~0x10; // DOWN
			}
			break;
		case 6:
			if (bOut) {
				if (KEY_SCODE(0x14)) data &= ~0x02; // Q
				if (KEY_SCODE(0x39)) data &= ~0x04; // CAPITAL
				if (KEY_SCODE(0x1D)) data &= ~0x08; // Z
				if (KEY_SCODE(0x48)) data &= ~0x10; // Break
			} else {
				if (KEY_SCODE(0x29)) data &= ~0x02; // ESCAPE
				if (KEY_SCODE(0x04)) data &= ~0x04; // A
				if (KEY_SCODE(0x1E)) data &= ~0x08; // 1
				if (KEY_SCODE(0xE4) ||
					KEY_SCODE(0xE0)) data &= ~0x10; // LCONTROL
			}
			break;
		case 7:
			if (bOut) {
				if (KEY_SCODE(0x24)) data &= ~0x02; // 7
				if (KEY_SCODE(0x1C)) data &= ~0x04; // Y
				if (KEY_SCODE(0x0E)) data &= ~0x08; // K
				if (KEY_SCODE(0x10)) data &= ~0x10; // M
			} else {
				if (KEY_SCODE(0x3D)) data &= ~0x02; // F4
				if (KEY_SCODE(0x18)) data &= ~0x04; // U
				if (KEY_SCODE(0x25)) data &= ~0x08; // 8
				if (KEY_SCODE(0x0D)) data &= ~0x10; // J
			}
			break;
		case 8:
			if (bOut) {
				if (KEY_SCODE(0x2D)) data &= ~0x02; // MINUS
				if (KEY_SCODE(0x33)) data &= ~0x04; // SEMICOLON
				if (KEY_SCODE(0x34)) data &= ~0x08; // APOSTROPHE
				if (KEY_SCODE(0x38)) data &= ~0x10; // SLASH
			} else {
				if (KEY_SCODE(0x3F)) data &= ~0x02; // F6
				if (KEY_SCODE(0x13)) data &= ~0x04; // P
				if (KEY_SCODE(0x2E)) data &= ~0x08; // EQUALS
				if (KEY_SCODE(0xE1) ||				// LSHIFT
					KEY_SCODE(0xE5)) data &= ~0x10; // RSHIFT
			}
			break;
		case 9:
			if (bOut) {
				if (KEY_SCODE(0x17)) data &= ~0x02; // T
				if (KEY_SCODE(0x0B)) data &= ~0x04; // H
				if (KEY_SCODE(0x11)) data &= ~0x08; // N
				if (KEY_SCODE(0x2C)) data &= ~0x10; // Space
			} else {
				if (KEY_SCODE(0x3C)) data &= ~0x02; // F3
				if (KEY_SCODE(0x15)) data &= ~0x04; // R
				if (KEY_SCODE(0x23)) data &= ~0x08; // 6
				if (KEY_SCODE(0x05)) data &= ~0x10; // B
			}
			break;
		case 10:
			if (bOut) {
			} else {
				data &= ~0x02;
			}
			break;
		case 11:
			if (bOut) {
		//		if (KEY_SCODE(0xXX)) data &= ~0x02; // LMENU
				if (KEY_SCODE(0x5C)) data &= ~0x04; // NUMPAD4
				if (KEY_SCODE(0x5F)) data &= ~0x08; // NUMPAD7
				if (KEY_SCODE(0x44)) data &= ~0x10; // F11
			} else {
				if (KEY_SCODE(0x45)) data &= ~0x02; // F12
				if (KEY_SCODE(0x59)) data &= ~0x04; // NUMPAD1
				if (KEY_SCODE(0x5A)) data &= ~0x08; // NUMPAD2
				if (KEY_SCODE(0x60)) data &= ~0x10; // NUMPAD8
			}
			break;
		case 12:
			if (bOut) {
				if (KEY_SCODE(0x56)) data &= ~0x02; // SUBTRACT
				if (KEY_SCODE(0x57)) data &= ~0x04; // ADD
				if (KEY_SCODE(0x55)) data &= ~0x08; // MULTIPLY
				if (KEY_SCODE(0x61)) data &= ~0x10; // NUMPAD9
			} else {
				if (KEY_SCODE(0x43)) data &= ~0x02; // F10
				if (KEY_SCODE(0x5D)) data &= ~0x04; // NUMPAD5
				if (KEY_SCODE(0x54)) data &= ~0x08; // DIVIDE
				if (KEY_SCODE(0x53)) data &= ~0x10; // NUMLOCK
			}
			break;
		case 13:
			if (bOut) {
				if (KEY_SCODE(0x35)) data &= ~0x02; // GRAVE
				if (KEY_SCODE(0x5E)) data &= ~0x04; // NUMPAD6
				if (KEY_SCODE(0xE2) ||			    // LALT
					KEY_SCODE(0xE6)) data &= ~0x08; // RALT
				if (KEY_SCODE(0x2B)) data &= ~0x10; // TAB
			} else {
				if (KEY_SCODE(0x42)) data &= ~0x02; // F9
				if (KEY_SCODE(0x5B)) data &= ~0x04; // NUMPAD3
				if (KEY_SCODE(0x63)) data &= ~0x08; // DECIMAL
				if (KEY_SCODE(0x62)) data &= ~0x10; // NUMPAD0
			}
			break;
	}

	return data;
}
