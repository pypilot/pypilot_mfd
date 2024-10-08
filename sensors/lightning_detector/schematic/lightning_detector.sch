EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L power:GND #PWR0112
U 1 1 5E3C9DB3
P 4350 4500
F 0 "#PWR0112" H 4350 4250 50  0001 C CNN
F 1 "GND" H 4250 4400 50  0001 C CNN
F 2 "" H 4350 4500 50  0001 C CNN
F 3 "" H 4350 4500 50  0001 C CNN
	1    4350 4500
	1    0    0    -1  
$EndComp
$Comp
L power:VCC #PWR0104
U 1 1 5F2D4813
P 4350 3900
F 0 "#PWR0104" H 4350 3750 50  0001 C CNN
F 1 "VCC" H 4367 4073 50  0000 C CNN
F 2 "" H 4350 3900 50  0001 C CNN
F 3 "" H 4350 3900 50  0001 C CNN
	1    4350 3900
	1    0    0    -1  
$EndComp
$Comp
L Device:Q_PMOS_GSD Q3
U 1 1 656AFBED
P 3650 4000
F 0 "Q3" V 3901 4000 50  0000 C CNN
F 1 "Q_PMOS_GSD" V 3901 4000 50  0001 C CNN
F 2 "pypilot_footprints:SOT-23_180" H 3850 4100 50  0001 C CNN
F 3 "~" H 3650 4000 50  0001 C CNN
F 4 "C10487" V 3650 4000 50  0001 C CNN "LCSC"
	1    3650 4000
	0    -1   -1   0   
$EndComp
$Comp
L Device:Q_PNP_BEC Q4
U 1 1 656B27F4
P 3750 4400
F 0 "Q4" H 3941 4400 50  0000 L CNN
F 1 "Q_PNP_BEC" H 3940 4355 50  0001 L CNN
F 2 "pypilot_footprints:SOT-23_180" H 3950 4500 50  0001 C CNN
F 3 "~" H 3750 4400 50  0001 C CNN
F 4 "C8543" H 3750 4400 50  0001 C CNN "LCSC"
	1    3750 4400
	1    0    0    1   
$EndComp
$Comp
L Device:Q_PNP_BEC Q2
U 1 1 656B7174
P 3350 4400
F 0 "Q2" H 3541 4400 50  0000 L CNN
F 1 "Q_PNP_BEC" H 3540 4355 50  0001 L CNN
F 2 "pypilot_footprints:SOT-23_180" H 3550 4500 50  0001 C CNN
F 3 "~" H 3350 4400 50  0001 C CNN
F 4 "C8543" H 3350 4400 50  0001 C CNN "LCSC"
	1    3350 4400
	-1   0    0    1   
$EndComp
Wire Wire Line
	3850 4200 3850 3900
Wire Wire Line
	3450 3900 3250 3900
Wire Wire Line
	3250 3900 3250 4200
$Comp
L pypilot_components:CJ431 U3
U 1 1 656BCC1D
P 2700 4550
F 0 "U3" V 2746 4480 50  0000 R CNN
F 1 "TL431DBZ" V 3100 4500 50  0000 R CNN
F 2 "pypilot_footprints:SOT-23_180" H 2700 4400 50  0001 C CIN
F 3 "http://www.ti.com/lit/ds/symlink/tl431.pdf" H 2700 4550 50  0001 C CIN
F 4 "C3113" V 2700 4550 50  0001 C CNN "LCSC"
	1    2700 4550
	0    -1   -1   0   
$EndComp
$Comp
L Device:R R17
U 1 1 656BE0C0
P 2600 4700
F 0 "R17" H 2400 4650 50  0000 L CNN
F 1 "33k" V 2600 4600 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 2530 4700 50  0001 C CNN
F 3 "" H 2600 4700 50  0001 C CNN
F 4 "C17633" H 2600 4700 50  0001 C CNN "LCSC"
	1    2600 4700
	1    0    0    -1  
$EndComp
$Comp
L Device:R R16
U 1 1 656BEC37
P 2600 4400
F 0 "R16" H 2400 4350 50  0000 L CNN
F 1 "15k" V 2600 4300 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 2530 4400 50  0001 C CNN
F 3 "" H 2600 4400 50  0001 C CNN
F 4 "C17475" H 2600 4400 50  0001 C CNN "LCSC"
	1    2600 4400
	1    0    0    -1  
$EndComp
Connection ~ 2600 4550
$Comp
L power:GND #PWR0124
U 1 1 656BF3A3
P 2600 4850
F 0 "#PWR0124" H 2600 4600 50  0001 C CNN
F 1 "GND" H 2750 4750 50  0001 C CNN
F 2 "" H 2600 4850 50  0001 C CNN
F 3 "" H 2600 4850 50  0001 C CNN
	1    2600 4850
	1    0    0    -1  
$EndComp
$Comp
L Device:R R18
U 1 1 656BF7DF
P 3250 4750
F 0 "R18" H 3050 4850 50  0000 L CNN
F 1 "2M" V 3250 4700 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 3180 4750 50  0001 C CNN
F 3 "" H 3250 4750 50  0001 C CNN
F 4 "C22976" H 3250 4750 50  0001 C CNN "LCSC"
	1    3250 4750
	1    0    0    -1  
$EndComp
$Comp
L Device:R R19
U 1 1 656BFFCC
P 3850 4750
F 0 "R19" H 3900 4900 50  0000 L CNN
F 1 "2M" V 3850 4700 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 3780 4750 50  0001 C CNN
F 3 "" H 3850 4750 50  0001 C CNN
F 4 "C22976" H 3850 4750 50  0001 C CNN "LCSC"
	1    3850 4750
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0125
U 1 1 656C0572
P 3250 4900
F 0 "#PWR0125" H 3250 4650 50  0001 C CNN
F 1 "GND" H 3400 4800 50  0001 C CNN
F 2 "" H 3250 4900 50  0001 C CNN
F 3 "" H 3250 4900 50  0001 C CNN
	1    3250 4900
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0126
U 1 1 656C0A27
P 3850 4900
F 0 "#PWR0126" H 3850 4650 50  0001 C CNN
F 1 "GND" H 4000 4800 50  0001 C CNN
F 2 "" H 3850 4900 50  0001 C CNN
F 3 "" H 3850 4900 50  0001 C CNN
	1    3850 4900
	1    0    0    -1  
$EndComp
Wire Wire Line
	3250 3900 2700 3900
Wire Wire Line
	2600 3900 2600 3950
Connection ~ 3250 3900
Connection ~ 3550 4400
Wire Wire Line
	3650 4200 3650 4600
Wire Wire Line
	3650 4600 3850 4600
Connection ~ 3850 4600
Wire Wire Line
	2700 4850 2600 4850
Connection ~ 2600 4850
Wire Wire Line
	2700 4450 2700 3900
Connection ~ 2700 3900
Wire Wire Line
	2700 3900 2600 3900
$Comp
L power:GND #PWR0127
U 1 1 656D50F7
P 2000 4300
F 0 "#PWR0127" H 2000 4050 50  0001 C CNN
F 1 "GND" H 2150 4200 50  0001 C CNN
F 2 "" H 2000 4300 50  0001 C CNN
F 3 "" H 2000 4300 50  0001 C CNN
	1    2000 4300
	1    0    0    -1  
$EndComp
Wire Wire Line
	2000 3900 2600 3900
Connection ~ 2600 3900
Connection ~ 3850 3900
Wire Wire Line
	3550 4600 3250 4600
Connection ~ 3250 4600
$Comp
L Connector_Generic:Conn_01x01 SC1
U 1 1 65B19B86
P 1800 3900
F 0 "SC1" H 1908 3946 50  0000 L CNN
F 1 "Solar_Cells" H 1350 3750 50  0000 L CNN
F 2 "Connector_Pin:Pin_D1.4mm_L8.5mm_W2.8mm_FlatFork" V 1800 3960 50  0001 C CNN
F 3 "~" V 1800 3960 50  0001 C CNN
F 4 "" H 1800 3900 50  0001 C CNN "LCSC"
	1    1800 3900
	-1   0    0    1   
$EndComp
Wire Wire Line
	2700 4850 2700 4650
$Comp
L Device:R R32
U 1 1 65A1099E
P 2600 4100
F 0 "R32" H 2400 4000 50  0000 L CNN
F 1 "2.4k" V 2600 4000 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 2530 4100 50  0001 C CNN
F 3 "" H 2600 4100 50  0001 C CNN
F 4 "C17526" H 2600 4100 50  0001 C CNN "LCSC"
	1    2600 4100
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0101
U 1 1 66637AE3
P 5750 6500
F 0 "#PWR0101" H 5750 6250 50  0001 C CNN
F 1 "GND" H 5755 6327 50  0001 C CNN
F 2 "" H 5750 6500 50  0001 C CNN
F 3 "" H 5750 6500 50  0001 C CNN
	1    5750 6500
	1    0    0    -1  
$EndComp
$Comp
L power:+3V3 #PWR0105
U 1 1 666397AD
P 5750 3300
F 0 "#PWR0105" H 5750 3150 50  0001 C CNN
F 1 "+3V3" H 5765 3473 50  0000 C CNN
F 2 "" H 5750 3300 50  0001 C CNN
F 3 "" H 5750 3300 50  0001 C CNN
	1    5750 3300
	1    0    0    -1  
$EndComp
$Comp
L Device:C C2
U 1 1 66636590
P 6050 3450
F 0 "C2" H 6165 3496 50  0000 L CNN
F 1 "100n" H 6165 3405 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 6088 3300 50  0001 C CNN
F 3 "~" H 6050 3450 50  0001 C CNN
F 4 "C49678" H 6050 3450 50  0001 C CNN "LCSC"
	1    6050 3450
	1    0    0    -1  
$EndComp
$Comp
L RF_Module:ESP32-WROOM-32 U1
U 1 1 666337CC
P 5750 5100
F 0 "U1" H 5500 6550 50  0000 C CNN
F 1 "ESP32-WROOM-32" H 5650 5650 50  0000 C CNN
F 2 "RF_Module:ESP32-WROOM-32" H 5750 3600 50  0001 C CNN
F 3 "https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf" H 5450 5150 50  0001 C CNN
	1    5750 5100
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0108
U 1 1 666369CB
P 6050 3600
F 0 "#PWR0108" H 6050 3350 50  0001 C CNN
F 1 "GND" H 6055 3427 50  0001 C CNN
F 2 "" H 6050 3600 50  0001 C CNN
F 3 "" H 6050 3600 50  0001 C CNN
	1    6050 3600
	1    0    0    -1  
$EndComp
Wire Wire Line
	4450 1850 4450 1750
Wire Wire Line
	4450 2150 4450 2400
$Comp
L Device:C C6
U 1 1 6667DA72
P 4000 2000
F 0 "C6" H 4115 2046 50  0000 L CNN
F 1 "10u" H 4115 1955 50  0000 L CNN
F 2 "Capacitor_SMD:C_1206_3216Metric" H 4038 1850 50  0001 C CNN
F 3 "~" H 4000 2000 50  0001 C CNN
F 4 "C13585" H 4000 2000 50  0001 C CNN "LCSC"
	1    4000 2000
	1    0    0    -1  
$EndComp
$Comp
L Device:C C9
U 1 1 666BE1D5
P 5850 2000
F 0 "C9" H 5965 2046 50  0000 L CNN
F 1 "10u" H 5965 1955 50  0000 L CNN
F 2 "Capacitor_SMD:C_1206_3216Metric" H 5888 1850 50  0001 C CNN
F 3 "~" H 5850 2000 50  0001 C CNN
F 4 "C13585" H 5850 2000 50  0001 C CNN "LCSC"
	1    5850 2000
	1    0    0    -1  
$EndComp
Wire Wire Line
	5850 2150 5850 2400
Wire Wire Line
	5850 1750 5850 1850
$Comp
L power:+3V3 #PWR0113
U 1 1 666C070E
P 5850 1750
F 0 "#PWR0113" H 5850 1600 50  0001 C CNN
F 1 "+3V3" H 5865 1923 50  0000 C CNN
F 2 "" H 5850 1750 50  0001 C CNN
F 3 "" H 5850 1750 50  0001 C CNN
	1    5850 1750
	1    0    0    -1  
$EndComp
Connection ~ 5850 1750
$Comp
L Connector_Generic:Conn_01x04 J4
U 1 1 666EACFE
P 7050 4200
F 0 "J4" H 7130 4192 50  0000 L CNN
F 1 "jst" H 7130 4101 50  0000 L CNN
F 2 "Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal" H 7050 4200 50  0001 C CNN
F 3 "~" H 7050 4200 50  0001 C CNN
F 4 "C160404" H 7050 4200 50  0001 C CNN "LCSC"
	1    7050 4200
	1    0    0    -1  
$EndComp
Wire Wire Line
	6600 4300 6600 4000
Wire Wire Line
	6600 4000 6350 4000
$Comp
L power:+3V3 #PWR0116
U 1 1 666F6B95
P 6850 4100
F 0 "#PWR0116" H 6850 3950 50  0001 C CNN
F 1 "+3V3" H 6700 4150 50  0000 C CNN
F 2 "" H 6850 4100 50  0001 C CNN
F 3 "" H 6850 4100 50  0001 C CNN
	1    6850 4100
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0117
U 1 1 666F70E9
P 6850 4400
F 0 "#PWR0117" H 6850 4150 50  0001 C CNN
F 1 "GND" H 6855 4227 50  0001 C CNN
F 2 "" H 6850 4400 50  0001 C CNN
F 3 "" H 6850 4400 50  0001 C CNN
	1    6850 4400
	1    0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J2
U 1 1 666FA729
P 7050 3700
F 0 "J2" H 7130 3692 50  0000 L CNN
F 1 "boot" H 7130 3601 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" H 7050 3700 50  0001 C CNN
F 3 "~" H 7050 3700 50  0001 C CNN
	1    7050 3700
	1    0    0    -1  
$EndComp
$Comp
L power:+3V3 #PWR0118
U 1 1 666FF493
P 6550 3400
F 0 "#PWR0118" H 6550 3250 50  0001 C CNN
F 1 "+3V3" H 6400 3450 50  0000 C CNN
F 2 "" H 6550 3400 50  0001 C CNN
F 3 "" H 6550 3400 50  0001 C CNN
	1    6550 3400
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0119
U 1 1 66700040
P 6850 3800
F 0 "#PWR0119" H 6850 3550 50  0001 C CNN
F 1 "GND" H 6855 3627 50  0001 C CNN
F 2 "" H 6850 3800 50  0001 C CNN
F 3 "" H 6850 3800 50  0001 C CNN
	1    6850 3800
	1    0    0    -1  
$EndComp
Wire Wire Line
	6850 3700 6550 3700
Wire Wire Line
	6350 3700 6350 3900
$Comp
L Device:R R4
U 1 1 66701F18
P 6550 3550
F 0 "R4" H 6400 3550 50  0000 L CNN
F 1 "10k" V 6550 3450 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 6480 3550 50  0001 C CNN
F 3 "~" H 6550 3550 50  0001 C CNN
F 4 "C17414" H 6550 3550 50  0001 C CNN "LCSC"
	1    6550 3550
	-1   0    0    1   
$EndComp
Connection ~ 6550 3700
Wire Wire Line
	6550 3700 6350 3700
Wire Wire Line
	6050 3300 5750 3300
Wire Wire Line
	5750 3300 5750 3700
Connection ~ 5750 3300
Wire Wire Line
	6350 4200 6850 4200
Wire Wire Line
	6600 4300 6850 4300
$Comp
L Device:C C5
U 1 1 6667D9F5
P 4450 2000
F 0 "C5" H 4565 2046 50  0000 L CNN
F 1 "100n" H 4565 1955 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 4488 1850 50  0001 C CNN
F 3 "~" H 4450 2000 50  0001 C CNN
F 4 "C49678" H 4450 2000 50  0001 C CNN "LCSC"
	1    4450 2000
	1    0    0    -1  
$EndComp
Wire Wire Line
	4000 1850 4000 1750
Connection ~ 4450 1750
Wire Wire Line
	4000 1750 4450 1750
Wire Wire Line
	4000 2150 4000 2400
Connection ~ 4450 2400
Wire Wire Line
	4000 2400 4450 2400
Wire Wire Line
	4450 1750 4750 1750
$Comp
L power:VCC #PWR0102
U 1 1 667ADC08
P 4000 1750
F 0 "#PWR0102" H 4000 1600 50  0001 C CNN
F 1 "VCC" H 4017 1923 50  0000 C CNN
F 2 "" H 4000 1750 50  0001 C CNN
F 3 "" H 4000 1750 50  0001 C CNN
	1    4000 1750
	1    0    0    -1  
$EndComp
Connection ~ 4000 1750
Wire Wire Line
	3850 3900 4350 3900
Wire Wire Line
	5100 3900 5150 3900
Text GLabel 6350 5200 2    50   Input ~ 0
MISO
Text GLabel 6350 5500 2    50   Input ~ 0
MOSI
Text GLabel 6350 5100 2    50   Input ~ 0
SCK
Text GLabel 6350 4400 2    50   Input ~ 0
SS
Text GLabel 6350 4500 2    50   Input ~ 0
INT0
$Comp
L Connector_Generic:Conn_01x11 J1
U 1 1 667C6FC3
P 8350 5200
F 0 "J1" H 8600 5400 50  0000 L CNN
F 1 "CJMCU-3935" H 8650 5200 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x11_P2.54mm_Vertical" H 8350 5200 50  0001 C CNN
F 3 "~" H 8350 5200 50  0001 C CNN
	1    8350 5200
	1    0    0    -1  
$EndComp
Text Notes 8400 4700 0    50   ~ 0
A1
Text Notes 8400 4800 0    50   ~ 0
A0
Text Notes 8400 4900 0    50   ~ 0
EN_V
Text Notes 8400 5000 0    50   ~ 0
IRQ
Text Notes 8400 5100 0    50   ~ 0
SI
Text Notes 8400 5200 0    50   ~ 0
CS
Text Notes 8400 5300 0    50   ~ 0
MISO
Text Notes 8400 5400 0    50   ~ 0
MOSI
Text Notes 8400 5500 0    50   ~ 0
SCL
Text Notes 8400 5600 0    50   ~ 0
GND
Text Notes 8400 5700 0    50   ~ 0
VCC
$Comp
L power:GND #PWR0103
U 1 1 667CBAE8
P 7950 5600
F 0 "#PWR0103" H 7950 5350 50  0001 C CNN
F 1 "GND" H 7955 5427 50  0001 C CNN
F 2 "" H 7950 5600 50  0001 C CNN
F 3 "" H 7950 5600 50  0001 C CNN
	1    7950 5600
	1    0    0    -1  
$EndComp
Wire Wire Line
	8150 5600 7950 5600
$Comp
L power:+3V3 #PWR0106
U 1 1 667CCBB3
P 8150 5700
F 0 "#PWR0106" H 8150 5550 50  0001 C CNN
F 1 "+3V3" V 8000 5750 50  0000 C CNN
F 2 "" H 8150 5700 50  0001 C CNN
F 3 "" H 8150 5700 50  0001 C CNN
	1    8150 5700
	0    -1   -1   0   
$EndComp
Text GLabel 8150 5500 0    50   Input ~ 0
SCK
Text GLabel 8150 5300 0    50   Input ~ 0
MISO
Text GLabel 8150 5400 0    50   Input ~ 0
MOSI
Text GLabel 8150 5200 0    50   Input ~ 0
SS
Text GLabel 8150 5000 0    50   Input ~ 0
INT0
Wire Wire Line
	4450 2400 5050 2400
$Comp
L Regulator_Linear:APE8865N-15-HF-3 U2
U 1 1 667DAE54
P 5050 1750
F 0 "U2" H 5050 1992 50  0000 C CNN
F 1 "TPS7A0533PDBZR" H 5050 1901 50  0000 C CNN
F 2 "pypilot_footprints:SOT-23_180" H 5050 1975 50  0001 C CIN
F 3 "http://www.tme.eu/fr/Document/ced3461ed31ea70a3c416fb648e0cde7/APE8865-3.pdf" H 5050 1750 50  0001 C CNN
F 4 "C2877921" H 5050 1750 50  0001 C CNN "LCSC"
	1    5050 1750
	1    0    0    -1  
$EndComp
$Comp
L Device:C C3
U 1 1 667DE8AC
P 5450 2000
F 0 "C3" H 5565 2046 50  0000 L CNN
F 1 "10u" H 5565 1955 50  0000 L CNN
F 2 "Capacitor_SMD:C_1206_3216Metric" H 5488 1850 50  0001 C CNN
F 3 "~" H 5450 2000 50  0001 C CNN
F 4 "C13585" H 5450 2000 50  0001 C CNN "LCSC"
	1    5450 2000
	1    0    0    -1  
$EndComp
Wire Wire Line
	5350 1750 5450 1750
Wire Wire Line
	5450 1850 5450 1750
Connection ~ 5450 1750
Wire Wire Line
	5450 1750 5850 1750
Wire Wire Line
	5450 2150 5450 2400
Connection ~ 5450 2400
Wire Wire Line
	5450 2400 5850 2400
Wire Wire Line
	5050 2050 5050 2400
Connection ~ 5050 2400
Wire Wire Line
	5050 2400 5450 2400
$Comp
L power:GND #PWR0107
U 1 1 667E68F7
P 7800 5100
F 0 "#PWR0107" H 7800 4850 50  0001 C CNN
F 1 "GND" H 7805 4927 50  0001 C CNN
F 2 "" H 7800 5100 50  0001 C CNN
F 3 "" H 7800 5100 50  0001 C CNN
	1    7800 5100
	1    0    0    -1  
$EndComp
Wire Wire Line
	7800 5100 8150 5100
Wire Wire Line
	7800 5100 7800 4900
Wire Wire Line
	7800 4900 8150 4900
Connection ~ 7800 5100
Text GLabel 6850 5300 2    50   Input ~ 0
SDA
Text GLabel 6850 5400 2    50   Input ~ 0
SCL
Wire Wire Line
	6850 5400 6650 5400
Wire Wire Line
	6850 5300 6800 5300
$Comp
L Device:R R1
U 1 1 667EAF13
P 5100 3700
F 0 "R1" H 4950 3700 50  0000 L CNN
F 1 "10k" V 5100 3600 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 5030 3700 50  0001 C CNN
F 3 "~" H 5100 3700 50  0001 C CNN
F 4 "C17414" H 5100 3700 50  0001 C CNN "LCSC"
	1    5100 3700
	-1   0    0    1   
$EndComp
Wire Wire Line
	5100 3850 5100 3900
Wire Wire Line
	5100 3550 5100 3300
Wire Wire Line
	5100 3300 5750 3300
$Comp
L Device:R R3
U 1 1 667ED2CF
P 6800 5150
F 0 "R3" H 6650 5150 50  0000 L CNN
F 1 "10k" V 6800 5050 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 6730 5150 50  0001 C CNN
F 3 "~" H 6800 5150 50  0001 C CNN
F 4 "C17414" H 6800 5150 50  0001 C CNN "LCSC"
	1    6800 5150
	-1   0    0    1   
$EndComp
Connection ~ 6800 5300
Wire Wire Line
	6800 5300 6350 5300
$Comp
L Device:R R2
U 1 1 667EE1DA
P 6650 5250
F 0 "R2" H 6500 5250 50  0000 L CNN
F 1 "10k" V 6650 5150 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 6580 5250 50  0001 C CNN
F 3 "~" H 6650 5250 50  0001 C CNN
F 4 "C17414" H 6650 5250 50  0001 C CNN "LCSC"
	1    6650 5250
	-1   0    0    1   
$EndComp
Connection ~ 6650 5400
Wire Wire Line
	6650 5400 6350 5400
Wire Wire Line
	6650 5100 6650 5000
Wire Wire Line
	6650 5000 6800 5000
$Comp
L power:+3V3 #PWR0109
U 1 1 667EF9F3
P 6800 5000
F 0 "#PWR0109" H 6800 4850 50  0001 C CNN
F 1 "+3V3" H 6650 5050 50  0000 C CNN
F 2 "" H 6800 5000 50  0001 C CNN
F 3 "" H 6800 5000 50  0001 C CNN
	1    6800 5000
	1    0    0    -1  
$EndComp
Connection ~ 6800 5000
$Comp
L Connector_Generic:Conn_01x06 J5
U 1 1 66868465
P 9300 2300
F 0 "J5" H 9380 2292 50  0000 L CNN
F 1 "AS7731" H 9380 2201 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical" H 9300 2300 50  0001 C CNN
F 3 "~" H 9300 2300 50  0001 C CNN
	1    9300 2300
	1    0    0    -1  
$EndComp
Text GLabel 9100 2100 0    50   Input ~ 0
SCL
Text GLabel 9100 2200 0    50   Input ~ 0
SDA
$Comp
L power:+3V3 #PWR0110
U 1 1 6686EA1B
P 9100 2300
F 0 "#PWR0110" H 9100 2150 50  0001 C CNN
F 1 "+3V3" V 9100 2550 50  0000 C CNN
F 2 "" H 9100 2300 50  0001 C CNN
F 3 "" H 9100 2300 50  0001 C CNN
	1    9100 2300
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR0111
U 1 1 668718A9
P 9100 2400
F 0 "#PWR0111" H 9100 2150 50  0001 C CNN
F 1 "GND" H 9105 2227 50  0001 C CNN
F 2 "" H 9100 2400 50  0001 C CNN
F 3 "" H 9100 2400 50  0001 C CNN
	1    9100 2400
	0    1    1    0   
$EndComp
Text GLabel 9100 2500 0    50   Input ~ 0
INT1
Text GLabel 9100 2600 0    50   Input ~ 0
SYNC
Text GLabel 6350 4600 2    50   Input ~ 0
INT1
Text GLabel 6350 4900 2    50   Input ~ 0
SYNC
$Comp
L Connector_Generic:Conn_01x04 J3
U 1 1 66882E81
P 9300 1350
F 0 "J3" H 9380 1342 50  0000 L CNN
F 1 "VEML7700" H 9380 1251 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical" H 9300 1350 50  0001 C CNN
F 3 "~" H 9300 1350 50  0001 C CNN
	1    9300 1350
	1    0    0    -1  
$EndComp
Text GLabel 9100 1550 0    50   Input ~ 0
SCL
Text GLabel 9100 1450 0    50   Input ~ 0
SDA
$Comp
L power:+3V3 #PWR0114
U 1 1 66882E8D
P 9100 1350
F 0 "#PWR0114" H 9100 1200 50  0001 C CNN
F 1 "+3V3" V 9100 1600 50  0000 C CNN
F 2 "" H 9100 1350 50  0001 C CNN
F 3 "" H 9100 1350 50  0001 C CNN
	1    9100 1350
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR0120
U 1 1 66882E97
P 9100 1250
F 0 "#PWR0120" H 9100 1000 50  0001 C CNN
F 1 "GND" H 9105 1077 50  0001 C CNN
F 2 "" H 9100 1250 50  0001 C CNN
F 3 "" H 9100 1250 50  0001 C CNN
	1    9100 1250
	0    1    1    0   
$EndComp
$Comp
L Connector_Generic:Conn_01x01 SC2
U 1 1 668B49EA
P 1800 4300
F 0 "SC2" H 1908 4346 50  0000 L CNN
F 1 "Solar_Cells" H 1350 4150 50  0001 L CNN
F 2 "Connector_Pin:Pin_D1.4mm_L8.5mm_W2.8mm_FlatFork" V 1800 4360 50  0001 C CNN
F 3 "~" V 1800 4360 50  0001 C CNN
F 4 "" H 1800 4300 50  0001 C CNN "LCSC"
	1    1800 4300
	-1   0    0    1   
$EndComp
$Comp
L Connector_Generic:Conn_01x01 BC2
U 1 1 668B4CA9
P 4550 4500
F 0 "BC2" H 4658 4546 50  0000 L CNN
F 1 "Battery" H 4100 4350 50  0001 L CNN
F 2 "Connector_Pin:Pin_D1.4mm_L8.5mm_W2.8mm_FlatFork" V 4550 4560 50  0001 C CNN
F 3 "~" V 4550 4560 50  0001 C CNN
F 4 "" H 4550 4500 50  0001 C CNN "LCSC"
	1    4550 4500
	1    0    0    1   
$EndComp
$Comp
L Connector_Generic:Conn_01x01 BC1
U 1 1 668B874F
P 4550 3900
F 0 "BC1" H 4658 3946 50  0000 L CNN
F 1 "Battery" H 4100 3750 50  0000 L CNN
F 2 "Connector_Pin:Pin_D1.4mm_L8.5mm_W2.8mm_FlatFork" V 4550 3960 50  0001 C CNN
F 3 "~" V 4550 3960 50  0001 C CNN
F 4 "" H 4550 3900 50  0001 C CNN "LCSC"
	1    4550 3900
	1    0    0    1   
$EndComp
Connection ~ 4350 3900
Wire Wire Line
	3550 4400 3550 4600
$Comp
L power:GND #PWR0115
U 1 1 668D222F
P 5050 2400
F 0 "#PWR0115" H 5050 2150 50  0001 C CNN
F 1 "GND" H 5055 2227 50  0001 C CNN
F 2 "" H 5050 2400 50  0001 C CNN
F 3 "" H 5050 2400 50  0001 C CNN
	1    5050 2400
	1    0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_01x07 J6
U 1 1 668D4A4C
P 8350 3550
F 0 "J6" H 8450 3750 50  0000 L CNN
F 1 "CJMCU-3935" H 8450 3450 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical" H 8350 3550 50  0001 C CNN
F 3 "~" H 8350 3550 50  0001 C CNN
	1    8350 3550
	1    0    0    -1  
$EndComp
Text GLabel 8150 3250 0    50   Input ~ 0
MOSI
Text GLabel 8150 3350 0    50   Input ~ 0
MISO
Text GLabel 8150 3450 0    50   Input ~ 0
SCK
Text GLabel 8150 3550 0    50   Input ~ 0
SS
Text GLabel 8150 3650 0    50   Input ~ 0
INT0
$Comp
L power:+3V3 #PWR0121
U 1 1 668D5E79
P 8150 3750
F 0 "#PWR0121" H 8150 3600 50  0001 C CNN
F 1 "+3V3" V 8150 4000 50  0000 C CNN
F 2 "" H 8150 3750 50  0001 C CNN
F 3 "" H 8150 3750 50  0001 C CNN
	1    8150 3750
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR0122
U 1 1 668D643F
P 8150 3850
F 0 "#PWR0122" H 8150 3600 50  0001 C CNN
F 1 "GND" H 8155 3677 50  0001 C CNN
F 2 "" H 8150 3850 50  0001 C CNN
F 3 "" H 8150 3850 50  0001 C CNN
	1    8150 3850
	0    1    1    0   
$EndComp
$Comp
L Switch:SW_DIP_x01 SW?
U 1 1 66B4BD2F
P 7000 5900
F 0 "SW?" H 7000 6167 50  0000 C CNN
F 1 "SW_DIP_x01" H 7000 6076 50  0000 C CNN
F 2 "" H 7000 5900 50  0001 C CNN
F 3 "~" H 7000 5900 50  0001 C CNN
	1    7000 5900
	1    0    0    -1  
$EndComp
Wire Wire Line
	6700 5900 6350 5900
$Comp
L power:GND #PWR?
U 1 1 66B4D3ED
P 7300 6100
F 0 "#PWR?" H 7300 5850 50  0001 C CNN
F 1 "GND" H 7305 5927 50  0001 C CNN
F 2 "" H 7300 6100 50  0001 C CNN
F 3 "" H 7300 6100 50  0001 C CNN
	1    7300 6100
	1    0    0    -1  
$EndComp
Wire Wire Line
	7300 6100 7300 5900
$Comp
L Device:R R?
U 1 1 66B51037
P 6700 5750
F 0 "R?" H 6550 5750 50  0000 L CNN
F 1 "10k" V 6700 5650 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric" V 6630 5750 50  0001 C CNN
F 3 "~" H 6700 5750 50  0001 C CNN
F 4 "C17414" H 6700 5750 50  0001 C CNN "LCSC"
	1    6700 5750
	-1   0    0    1   
$EndComp
Connection ~ 6700 5900
$Comp
L power:+3V3 #PWR?
U 1 1 66B51369
P 6700 5600
F 0 "#PWR?" H 6700 5450 50  0001 C CNN
F 1 "+3V3" H 6800 5600 50  0000 C CNN
F 2 "" H 6700 5600 50  0001 C CNN
F 3 "" H 6700 5600 50  0001 C CNN
	1    6700 5600
	1    0    0    -1  
$EndComp
$EndSCHEMATC
