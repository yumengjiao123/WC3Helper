#ifndef IWAR3PLUGIN
#define IWAR3PLUGIN

//#include "game/warcraft.h"

class IPlugin {
public:
	IPlugin() = default;
	~IPlugin() {};

	virtual void Start() = 0;
	virtual void Stop() = 0;
};

#endif//IWAR3PLUGIN
