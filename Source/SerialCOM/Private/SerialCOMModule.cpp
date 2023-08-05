#include "SerialCOMModule.h"

IMPLEMENT_MODULE(SerialCOMModule, SerialCom);

#define LOCTEXT_NAMESPACE "SerialCOM"

SerialCOMModule::SerialCOMModule()
{
}

void SerialCOMModule::StartupModule()
{
	// Startup LOG MSG
	UE_LOG(SerialComLog, Warning, TEXT("SerialCom: Log Started"));
}

void SerialCOMModule::ShutdownModule()
{
	// Shutdown LOG MSG
	UE_LOG(SerialComLog, Warning, TEXT("SerialCom: Log Ended"));
}

#undef LOCTEXT_NAMESPACE
