#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_CLASS(SerialComLog, Log, All);

class SerialCOMModule : public IModuleInterface
{
private:

public:
	SerialCOMModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
