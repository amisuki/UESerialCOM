//Based off the "Arduino and C++ (for Windows)" code found at: http://playground.arduino.cc/Interfacing/CPPWindows

#include "SerialCOM.h"
#include "SerialCOMModule.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/MinWindows.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif


#define BOOL2bool(B) B == 0 ? false : true

USerialCOM* USerialCOM::OpenComPortWithFlowControl(bool& bOpened, int32 Port, int32 BaudRate, bool DTR, bool RTS)
{
	USerialCOM* Serial = NewObject<USerialCOM>();
	bOpened = Serial->OpenWFC(Port, BaudRate, DTR, RTS);
	return Serial;
}

USerialCOM* USerialCOM::OpenComPort(bool& bOpened, int32 Port, int32 BaudRate)
{
	USerialCOM* Serial = NewObject<USerialCOM>();
	bOpened = Serial->OpenWFC(Port, BaudRate);
	return Serial;
}

int32 USerialCOM::BytesToInt(TArray<uint8> Bytes)
{
	if (Bytes.Num() != 4)
	{
		return 0;
	}

	return *reinterpret_cast<int32*>(Bytes.GetData());
}

TArray<uint8> USerialCOM::IntToBytes(const int32& Int)
{
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(&Int), 4);
	return Bytes;
}


float USerialCOM::BytesToFloat(TArray<uint8> Bytes)
{
	if (Bytes.Num() != 4)
	{
		return 0;
	}

	return *reinterpret_cast<float*>(Bytes.GetData());
}


TArray<uint8> USerialCOM::FloatToBytes(const float& Float)
{
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(&Float), 4);
	return Bytes;
}


USerialCOM::USerialCOM()
{
	// Allocate the OVERLAPPED structs
	OverlappedRead = new OVERLAPPED();
	OverlappedWrite = new OVERLAPPED();
	
	FMemory::Memset(OverlappedRead, 0, sizeof(OVERLAPPED));
	FMemory::Memset(OverlappedWrite, 0, sizeof(OVERLAPPED));
}

USerialCOM::~USerialCOM()
{
	Close();

	// Delete allocated OVERLAPPED structs
	delete OverlappedRead;
	delete OverlappedWrite;
}

bool USerialCOM::OpenWFC(int32 nPort, int32 nBaud, bool bDTR, bool bRTS)
{
	if (nPort < 0)
	{
		UE_LOG(SerialComLog, Error, TEXT("Invalid port number: %d"), nPort);
		return false;
	}
	if (m_hIDComDev)
	{
		UE_LOG(SerialComLog, Warning, TEXT("Trying to use opened Serial instance to open a new one. "
			       "Current open instance port: %d | Port tried: %d"), Port, nPort);
		return false;
	}

	FString szPort;
	if (nPort < 10)
		szPort = FString::Printf(TEXT("COM%d"), nPort);
	else
		szPort = FString::Printf(TEXT("\\\\.\\COM%d"), nPort);
	DCB dcb;

	m_hIDComDev = CreateFile(*szPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
	                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	if (m_hIDComDev == NULL)
	{
		unsigned long dwError = GetLastError();
		UE_LOG(SerialComLog, Error, TEXT("Failed to open port COM%d (%s). Error: %08X"), nPort, *szPort, dwError);
		return false;
	}

	FMemory::Memset(OverlappedRead, 0, sizeof(OVERLAPPED));
	FMemory::Memset(OverlappedWrite, 0, sizeof(OVERLAPPED));

	COMMTIMEOUTS CommTimeOuts;
	//CommTimeOuts.ReadIntervalTimeout = 10;
	CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF;
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
	CommTimeOuts.ReadTotalTimeoutConstant = 0;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0;
	CommTimeOuts.WriteTotalTimeoutConstant = 10;
	SetCommTimeouts(m_hIDComDev, &CommTimeOuts);

	OverlappedRead->hEvent = CreateEvent(NULL, true, false, NULL);
	OverlappedWrite->hEvent = CreateEvent(NULL, true, false, NULL);

	dcb.DCBlength = sizeof(DCB);
	GetCommState(m_hIDComDev, &dcb);
	dcb.BaudRate = nBaud;
	dcb.ByteSize = 8;
	if (bDTR == true)
	{
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
	}
	else
	{
		dcb.fDtrControl = DTR_CONTROL_DISABLE;
	}

	if (bRTS == true)
	{
		dcb.fRtsControl = RTS_CONTROL_ENABLE;
	}
	else
	{
		dcb.fRtsControl = RTS_CONTROL_DISABLE;
	}

	if (!SetCommState(m_hIDComDev, &dcb) ||
		!SetupComm(m_hIDComDev, 10000, 10000) ||
		OverlappedRead->hEvent == NULL ||
		OverlappedWrite->hEvent == NULL)
	{
		unsigned long dwError = GetLastError();
		if (OverlappedRead->hEvent != NULL) CloseHandle(OverlappedRead->hEvent);
		if (OverlappedWrite->hEvent != NULL) CloseHandle(OverlappedWrite->hEvent);
		CloseHandle(m_hIDComDev);
		m_hIDComDev = NULL;
		UE_LOG(SerialComLog, Error, TEXT("Failed to setup port COM%d. Error: %08X"), nPort, dwError);
		return false;
	}

	//FPlatformProcess::Sleep(0.05f);
	AddToRoot();
	Port = nPort;
	Baud = nBaud;
	return true;
}

void USerialCOM::Close()
{
	if (!m_hIDComDev) return;

	if (OverlappedRead->hEvent != NULL) CloseHandle(OverlappedRead->hEvent);
	if (OverlappedWrite->hEvent != NULL) CloseHandle(OverlappedWrite->hEvent);
	CloseHandle(m_hIDComDev);
	m_hIDComDev = NULL;

	RemoveFromRoot();
}

FString USerialCOM::ReadString(bool& bSuccess)
{
	return ReadStringUntil(bSuccess, '\0');
}

FString USerialCOM::Readln(bool& bSuccess)
{
	return ReadStringUntil(bSuccess, '\n');
}

FString USerialCOM::ReadStringUntil(bool& bSuccess, uint8 Terminator)
{
	bSuccess = false;
	if (!m_hIDComDev) return TEXT("");

	TArray<uint8> Chars;
	uint8 Byte = 0x0;
	bool bReadStatus;
	unsigned long dwBytesRead, dwErrorFlags;
	COMSTAT ComStat;

	ClearCommError(m_hIDComDev, &dwErrorFlags, &ComStat);
	if (!ComStat.cbInQue) return TEXT("");

	do
	{
		bReadStatus = BOOL2bool(ReadFile(
			m_hIDComDev,
			&Byte,
			1,
			&dwBytesRead,
			OverlappedRead));

		if (!bReadStatus)
		{
			if (GetLastError() == ERROR_IO_PENDING)
			{
				WaitForSingleObject(OverlappedRead->hEvent, 2000);
			}
			else
			{
				Chars.Add(0x0);
				break;
			}
		}

		if (Byte == Terminator || dwBytesRead == 0)
		{
			// when Terminator is \n, we know we're expecting lines from Arduino. But those
			// are ended in \r\n. That means that if we found the line Terminator (\n), our previous
			// character could be \r. If it is, we remove that from the array.
			if (Chars.Num() > 0 && Terminator == '\n' && Chars.Top() == '\r') Chars.Pop(false);

			Chars.Add(0x0);
			break;
		}
		else Chars.Add(Byte);
	}
	while (Byte != 0x0 && Byte != Terminator);

	bSuccess = true;
	const auto Convert = FUTF8ToTCHAR((ANSICHAR*)Chars.GetData());
	return FString(Convert.Get());
}

float USerialCOM::ReadFloat(bool& bSuccess)
{
	bSuccess = false;

	TArray<uint8> Bytes = ReadBytes(4);
	if (Bytes.Num() == 0) return 0;

	bSuccess = true;
	return *(reinterpret_cast<float*>(Bytes.GetData()));
}

int32 USerialCOM::ReadInt(bool& bSuccess)
{
	bSuccess = false;

	TArray<uint8> Bytes = ReadBytes(4);
	if (Bytes.Num() == 0) return 0;

	bSuccess = true;
	return *(reinterpret_cast<int32*>(Bytes.GetData()));
}

uint8 USerialCOM::ReadByte(bool& bSuccess)
{
	bSuccess = false;
	if (!m_hIDComDev) return 0x0;

	uint8 Byte = 0x0;
	bool bReadStatus;
	unsigned long dwBytesRead, dwErrorFlags;
	COMSTAT ComStat;

	ClearCommError(m_hIDComDev, &dwErrorFlags, &ComStat);
	if (!ComStat.cbInQue) return 0x0;

	bReadStatus = BOOL2bool(ReadFile(
		m_hIDComDev,
		&Byte,
		1,
		&dwBytesRead,
		OverlappedRead));

	if (!bReadStatus)
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			WaitForSingleObject(OverlappedRead->hEvent, 2000);
		}
		else
		{
			return 0x0;
		}
	}

	bSuccess = dwBytesRead > 0;
	return Byte;
}

TArray<uint8> USerialCOM::ReadBytes(int32 Limit)
{
	TArray<uint8> Data;

	if (!m_hIDComDev) return Data;

	Data.Empty(Limit);

	uint8* Buffer = new uint8[Limit];
	bool bReadStatus;
	unsigned long dwBytesRead, dwErrorFlags;
	COMSTAT ComStat;

	ClearCommError(m_hIDComDev, &dwErrorFlags, &ComStat);
	if (!ComStat.cbInQue) return Data;

	bReadStatus = BOOL2bool(ReadFile(
		m_hIDComDev,
		Buffer,
		Limit,
		&dwBytesRead,
		OverlappedRead));

	if (!bReadStatus)
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			WaitForSingleObject(OverlappedRead->hEvent, 2000);
		}
		else
		{
			return Data;
		}
	}

	Data.Append(Buffer, dwBytesRead);
	return Data;
}

bool USerialCOM::Print(FString String)
{
	auto Convert = FTCHARToUTF8(*String);
	TArray<uint8> Data;
	Data.Append((uint8*)Convert.Get(), Convert.Length());

	return WriteBytes(Data);
}

bool USerialCOM::Println(FString String)
{
	return Print(String + LineEndToStr(WriteLineEnd));
}

bool USerialCOM::WriteFloat(float Value)
{
	TArray<uint8> Buffer;
	Buffer.Append(reinterpret_cast<uint8*>(&Value), 4);
	return WriteBytes(Buffer);
}

bool USerialCOM::WriteInt(int32 Value)
{
	TArray<uint8> Buffer;
	Buffer.Append(reinterpret_cast<uint8*>(&Value), 4);
	return WriteBytes(Buffer);
}

bool USerialCOM::WriteByte(uint8 Value)
{
	TArray<uint8> Buffer({Value});
	return WriteBytes(Buffer);
}

bool USerialCOM::WriteBytes(TArray<uint8> Buffer)
{
	if (!m_hIDComDev) false;

	bool bWriteStat;
	unsigned long dwBytesWritten;

	bWriteStat = BOOL2bool(WriteFile(m_hIDComDev, Buffer.GetData(), Buffer.Num(), &dwBytesWritten, OverlappedWrite));
	if (!bWriteStat && (GetLastError() == ERROR_IO_PENDING))
	{
		if (WaitForSingleObject(OverlappedWrite->hEvent, 1000))
		{
			dwBytesWritten = 0;
			return false;
		}
		else
		{
			GetOverlappedResult(m_hIDComDev, OverlappedWrite, &dwBytesWritten, false);
			OverlappedWrite->Offset += dwBytesWritten;
			return true;
		}
	}

	return true;
}

void USerialCOM::Flush()
{
	if (!m_hIDComDev) return;

	TArray<uint8> Data;

	do
	{
		Data = ReadBytes(8192);
	}
	while (Data.Num() > 0);
}

FString USerialCOM::LineEndToStr(ELineEnd LineEnd)
{
	switch (LineEnd)
	{
	case ELineEnd::rn:
		return TEXT("\r\n");
	case ELineEnd::n:
		return TEXT("\n");
	case ELineEnd::r:
		return TEXT("\r");
	case ELineEnd::nr:
		return TEXT("\n\r");
	default:
		return TEXT("null");
	}
}

FString USerialCOM::ConvertBytesToString(const TArray<uint8>& In, int32 Count)
{
	return BytesToString(In.GetData(), Count);
}

FString USerialCOM::ConvertBytesToHex(const TArray<uint8>& In, int32 NumBytes)
{
	return BytesToHex(In.GetData(), NumBytes);
}


/*
FString USerialCom::LineEndToStrBD(ELineEnd LineEnd)
{
	switch (LineEnd)
	{
	case ELineEnd::A:
		return TEXT("150");
	case ELineEnd::B:
		return TEXT("200");
	case ELineEnd::C:
		return TEXT("300");
	case ELineEnd::D:
		return TEXT("600");
	case ELineEnd::E:
		return TEXT("1200");
/*		
	case ELineEnd::1800:
		return TEXT("1800");
	case ELineEnd::2400:
		return TEXT("2400");
	case ELineEnd::4800:
		return TEXT("4800");
	case ELineEnd::9600:
		return TEXT("9600");
	case ELineEnd::19200:
		return TEXT("19200");
	case ELineEnd::28800:
		return TEXT("28800");
	case ELineEnd::38400:
		return TEXT("38400");
	case ELineEnd::57600:
		return TEXT("57600");
	case ELineEnd::76800:
		return TEXT("76800");
	case ELineEnd::115200:
		return TEXT("115200");
	case ELineEnd::230400:
		return TEXT("230400");
	case ELineEnd::460800:
		return TEXT("460800");
	case ELineEnd::576000:
		return TEXT("576000");
	case ELineEnd::921600:
		return TEXT("921600");
*/

/*
	default:
		return TEXT("9600");
	}
}
*/
