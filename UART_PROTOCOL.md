# SID HPD Protocol
> [Original document by Leigh Oliver](https://github.com/leighleighleigh/saab-93NG-HPD-hacker/blob/main/UART_PROTOCOL.md?plain=1#frame-format)

This document is incomplete and 

Thanks to users **ruthenianboy** and **bojer** of trionictuning.com for their work in reverse-engineering the protocol of the SID.

And from me (KickingAnimal) thanks to **LeighLeighLeigh** for having open source code available.

## Physical Interface

- **115200baud** UART over CAN physical layer.

## Communication sequence

1. ICM sends command to SID
2. SID replies with ACK/ERROR or other frame. 
3. SID ALWAYS replies.

## Frame format

| 0x4    | 0x70       | 0x0  | 0x2            | 0x0            | 0x0         |
| ------ | ---------- | ---- | -------------- | -------------- | ----------- |
| DLC(1) | COMMAND(1) | 0x0  | DATA(DLC-2)[0] | DATA(DLC-2)[1] | CHECKSUM(1) |

> CHECKSUM is the SUM of all the bytes excluding the checksum itself.
>
> If calculated checksum is larger than FF, example 0x9E1 checksum to send/compare is 0xE1.
>
> Example from OK response
>
> Data seen on UART: 0x02, 0xFF, 0x00, 0x01
>
> 0x2 + 0xFF + 0x0 = 0x101 = 0x01 matches checksum.


## Commands table

| COMMAND BYTE | DATA BYTES DESCRIPTIONS                                      | RESPONSE | DESCRIPTION                                        |
| ------------ | ------------------------------------------------------------ | -------- | -------------------------------------------------- |
| 0x80         | [0]: Green LED brightness<br />[1]: Orange/RED LED brightness  | OK       | Adjusts backlight.                                 |
| 0x81         |                                                              | 0x82...  | Status?                                            |
| 0x83         |                                                              | 0x84...  | Status?  get brightness?                           |
| 0x10         | [0]: Region ID<br/>[2:3]: Sub-region ID <br/>[4]: layer visibility??? Usually 0x0<br/><br />[5]: Font Style<br/>    # 0x00 Small<br/>    # 0x01 Large<br/>    # 0x02 Medium (normal)<br/>    # 0x04/0x14 Time (left 2 time digits are 0x04, the right are 0x14)<br />[6]: Width<br/>[8]: X Position<br/>[10]: Y Position<br/>[11+]: Text data | OK       | Setup layer on region, with optional text data.             |
| 0x11         | [0]: Region ID<br/>[2:3]: Sub Region ID<br/>[4]: Layer visibility<br/>    # 0x00 Invisible<br/>    # 0x03 Invisible2?<br/>    # 0x02 Active<br/>    # 0x08 Active2?<br/>[5]: Style<br/>    # 0x00, normal<br/>    # 0x10, right aligned<br/>    # 0x20, blinking<br/>    # 0x40, inverted <br/>    # 0x80, underline<br />[6+]NEW text data | OK       | Adjust layer presentation and optional text data.                         |
| 0x60         | [0]: Region ID<br />[2]: Clear Flag (0x0)                    | OK       | CLEAR region from SID/HPD memory.                                      |
| 0x70         | [0]: Region ID<br />[2]: Draw Flag (0 or 1)                  | OK       | DRAW/HIDE region. Must be called after creating new region(s)? <br/>(0x00 hides the region (i.e. nightmode) 0x01 draws the region)                                       |
| 0x30         | [0]: Region ID<br/> [2:3]: Sub-region ID<br/> [4]: Icon Number<br/> [5]: Icon Style<br/> [6]: X Pos<br/> [7]: Right Align |          | Setup ICON                                         |
| 0x33         | [0]: Region ID<br/> [2:3]: Sub-region ID<br/> [4:5]: Car door state<br/> [6]: X Pos<br/> [7]: Must be 0 else no works. |          | Draw CAR door stats                                |
| 0x40         |                                                              |          | Some kind of raw display stuff, does weird things. |
| 0xA0         |                                                              | None seen        | Not sure? Related to initial setup of SID?<br/> ICM sends after 0x81 and 0x83 have responded unsucesfully?<br/> maybe data reset command?         |
| 0xC0         |                                                              |          | Turns SID display OFF. (sent byt ICM before power down. |
| 0x20         |                                                              |          | Related to creating groups of sub-regions.         |
| 0x21         |                                                              |          | Related to moving groups of sub-regions.           |
|              |                                                              |          |                                                    |



- 0x80,0x80 clear screen / screen off
- 0x80,0x00,0xCE,0x00 (sets backlight for orange portion, last value is brightness)
      0xC, shows only the orange portion.

- 0x80,0x00-x79 screen on, brightess command?
- 0x81,0x00 (gets info from SID, status query?)
- 0x83,0x00 (gets some kind of info from SID, status query?)
- 0x11 < draw data to screen

- 0x70,0x0,0x76,0x0,0x01 < ~~Displays brake light failure icon~~ (Draw region ID 0x76)

- 0x94, some kind of serial code

        - 0x95 as response
- 0x96, some other kind of serial code

        - 0x97 as response

- 0x9f < self test mode (0x01,0x9f,0xA0)
- 0xC0 Sent on poweroff of ICM

        - Turns SID display off. Data continues to be recieved and answerd by sid

        - how to turn back on? 

Return values

- 0xff,0x00 (success) (DLC 0x02, COMMAND 0xFF, DATA 0x00, CHECKSUM 0x01)
- 0xfe,0x00,0x31 (invalid commmand / command doesn't exist)
- 0xfe,0x00,0x33 (Region already exists?) / From memory: seen when sending 0x10 with existing region+subregion
- 0xfe,0x00,0x34 (invalid command length, arguments?)
- 0xfe,0x00,0x35 ( no idea)
- 0xfe,0x0,0x37 (no idea)



## MISC

- Panel resolution (green) is 384x64 px
- Icon resolution (orange) is 64x64 px
- Temp and light sensors
- 4(5?) font sizes: Small; 0x00, Large; 0x01, Medium (Normal); 0x02, Time (0x04/0x14) 
- 4 font styles: underscore, solid background, flashing, and align right styles. Can be combined (see command table 0x11)

## ICON DATA CODES (DECIMAL):

- dec	desc		
- 65	!		
- 66	washer 	?	
- 67	wipers !		
- 68	CRUISE		
- 69	BREAK Park		
- 70	BREAK !		
- 71	BREAK ABS		
- 72	BATTERY		
- 73	COOLANT		
- 74	OIL temp		
- 75	OIL		
- 76	OIL level		
- 77	ENGINE		
- 78	CAR		
- 79	DOOR FL		
- 80	DOOR RL		
- 81	DOOR FR		
- 82	DOOR RR		
- 83	BOOT		
- 84	LIGHT		
- 85	AHL !		
- 86	GEARBOX !		
- 87	GEARBOX temp		
- 88	TURTLE	???	
- 89	SERVICE		
- 90	PASS		
- 91	SRS		
- 92	KEY		
- 93	KEY battery		
- 94	LOCK !		
- 95	CAR LOCK !		
- 96	SEATS rear		
- 97	"P"		
- 98	SPA on	???	
- 99	SPA off	???	
- 100	SPA !		
- 101	Tire !		
- 102	Tire dot		
- 103	STP		
- 104	SEATS front		
- 105	SCL lock		
- 106	SCL !		
- 107	ESP on		
- 108	ESP off		
- 109	STP closed	???	
- 110	STP !		
- 111	STP down	???	
- 112	SNOW/ACC off	???	
- 113	Fuel lid open	???	
- 114	NAV up		arrow
- 115	NAV up right		
- 116	NAV up left		
- 117	NAV UP		filled
- 118	NAV UP RIGHT		filled
- 119	NAV UP LEFT		filled
- 120	NAV HALF SKEW RIGHT		filled
- 121	NAV HALF SKEW LEFT		filled
- 122	NAV RIGHT		filled
- 123	NAV LEFT		filled
- 124	NAV RIGHT DOWN		filled
- 125	NAV LEFT DOWN		filled
- 126	NAV RETURN right		filled
- 127	NAV RETURN left		filled
- 128	NAV DOT		
- 129-135	NAV G right		arrow filled
- 136-142	NAV G left		arrow filled
- 143-156	NAV G background		
- 157	NAV TRIANGLE UP		filled
- 158	NAV UP target		filled
- 159	NAV WAYPOINT		filled
- 160	NAV up long		
- 161	NAV skew up right		
- 162	NAV right		
- 163	NAV skew down right		
- 164	NAV down		
- 165	NAV skew down left		
- 166	NAV left		
- 167	NAV skew up left		
- 168	NAV ROTATE LEFT		filled
- 169	NAV ROTATE RIGHT		filled
