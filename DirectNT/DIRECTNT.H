// directnt.h  Schnittstelle zwischen Treiber und Applikation
#define DIRECTNT_TYPE               40000
#define IOCTL_DIRECTNT_CONTROL      CTL_CODE(DIRECTNT_TYPE,0x0800, METHOD_BUFFERED,FILE_READ_ACCESS)

#define OP_Check          1 // gibt immer $12345678 zurueck
#define OP_GetCR0         2 // gibt Inhalt von Register CR0 zurueck
#define OP_SetCR0         3 // schreibt Inhalt in Register CR0
#define OP_ReadPortByte  10 // gibt Byte-Inhalt von Port #Par1# zurueck
#define OP_WritePortByte 11 // schreibt Byte #Par2# nach Port #Par1#
#define OP_GetMR         20 // Liest MSR nr #Par1# aus, =>64bittig
#define OP_SetMR         21 // schreibt MSR #Par1#,
                            // #PAR2#= untere 32Bit #Par3#=obere 32 Bit
#define OP_ReadMemDword  30 // Ein lineares Dword vom Speicher lesen
#define	OP_ReadPhysMemDword 40 // Ein physikalisches Dword vom Speicher lesen 
#define OP_GiveIO        50 // schaltet Ports #Par1# bis #Par2# frei
#define OP_LoadIOPM      51 // laedt IOPM => pointer auf 8 KByte Daten
#define OP_LockIO        52 // blockiert Ports #Par1# bis #Par2#
#define OP_ReadPCIDword  60 // liest PCI-Register #Par1# auf
                            // BUS/DEV/FKN  #Par2#
#define OP_WritePCIDword 61 // schreibt PCI-Register #Par1# auf
			    //  BUS/DEV/FKN #Par2# den Wert #Par3#
#define OP_ReadFlags	 99	// Testfunktion, Flags im Treiber abfragen 
typedef struct              //  Struktur der Input-Parameter
{
  ULONG OpCode;
  ULONG Par1;
  ULONG Par2;
  ULONG Par3;
} TDirectNTInfo,* PDirectNTInfo;

